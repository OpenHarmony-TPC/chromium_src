// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ohos/shared_image_video_ohos_native_image.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

SharedImageVideoOhosNativeImage::SharedImageVideoOhosNativeImage(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state)
    : SharedImageVideoOhos(mailbox,
                           size,
                           color_space,
                           surface_origin,
                           alpha_type,
                           /*is_thread_safe=*/false),
      stream_texture_sii_(std::move(stream_texture_sii)),
      context_state_(std::move(context_state)),
      gpu_main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(stream_texture_sii_);
  DCHECK(context_state_);

  context_state_->AddContextLostObserver(this);
}

SharedImageVideoOhosNativeImage::~SharedImageVideoOhosNativeImage() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_) {
    context_state_->RemoveContextLostObserver(this);
  }
  context_state_.reset();
  stream_texture_sii_->ReleaseResources();
  stream_texture_sii_.reset();
}

size_t SharedImageVideoOhosNativeImage::EstimatedSizeForMemTracking() const {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  return stream_texture_sii_->IsUsingGpuMemory() ? estimated_size() : 0;
}

void SharedImageVideoOhosNativeImage::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  stream_texture_sii_->ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

class SharedImageVideoOhosNativeImage::SharedImageRepresentationGLTextureVideo
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureVideo(
      SharedImageManager* manager,
      SharedImageVideoOhosNativeImage* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> texture)
      : SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(std::move(texture)) {}

  SharedImageRepresentationGLTextureVideo(
      const SharedImageRepresentationGLTextureVideo&) = delete;
  SharedImageRepresentationGLTextureVideo& operator=(
      const SharedImageRepresentationGLTextureVideo&) = delete;

  gles2::Texture* GetTexture() override {
    auto* texture = gles2::Texture::CheckedCast(texture_->GetTextureBase());
    DCHECK(texture);

    return texture;
  }

  bool BeginAccess(GLenum mode) override {
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<SharedImageVideoOhosNativeImage*>(backing());
    video_backing->BeginGLReadAccess(texture_->service_id());
    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<gles2::AbstractTexture> texture_;
};

class SharedImageVideoOhosNativeImage::
    SharedImageRepresentationGLTexturePassthroughVideo
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughVideo(
      SharedImageManager* manager,
      SharedImageVideoOhosNativeImage* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<gles2::AbstractTexture> abstract_texture)
      : SharedImageRepresentationGLTexturePassthrough(manager,
                                                      backing,
                                                      tracker),
        abstract_texture_(std::move(abstract_texture)),
        passthrough_texture_(gles2::TexturePassthrough::CheckedCast(
            abstract_texture_->GetTextureBase())) {
    CHECK(passthrough_texture_);
  }

  SharedImageRepresentationGLTexturePassthroughVideo(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;
  SharedImageRepresentationGLTexturePassthroughVideo& operator=(
      const SharedImageRepresentationGLTexturePassthroughVideo&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return passthrough_texture_;
  }

  bool BeginAccess(GLenum mode) override {
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    auto* video_backing =
        static_cast<SharedImageVideoOhosNativeImage*>(backing());
    video_backing->BeginGLReadAccess(passthrough_texture_->service_id());
    return true;
  }

  void EndAccess() override {}

 private:
  std::unique_ptr<gles2::AbstractTexture> abstract_texture_;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
};

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageVideoOhosNativeImage::ProduceGLTexture(SharedImageManager* manager,
                                                  MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (!stream_texture_sii_->HasTextureOwner()) {
    return nullptr;
  }

  auto texture = GenAbstractTexture(/*passthrough=*/false);
  if (!texture) {
    return nullptr;
  }

  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageVideoOhosNativeImage::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (!stream_texture_sii_->HasTextureOwner()) {
    return nullptr;
  }

  auto texture = GenAbstractTexture(/*passthrough=*/true);
  if (!texture) {
    return nullptr;
  }

  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  return std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
      manager, this, tracker, std::move(texture));
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageVideoOhosNativeImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_state);

  if (!stream_texture_sii_->HasTextureOwner()) {
    return nullptr;
  }

  if (!context_state->GrContextIsGL()) {
    DCHECK(false);
    return nullptr;
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture_base = stream_texture_sii_->GetTextureBase();
  DCHECK(texture_base);
  const bool passthrough =
      (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough);

  auto texture = GenAbstractTexture(passthrough);
  if (!texture) {
    return nullptr;
  }

  DCHECK(stream_texture_sii_->TextureOwnerBindsTextureOnUpdate());
  texture->BindStreamTextureImage(
      stream_texture_sii_.get(),
      stream_texture_sii_->GetTextureBase()->service_id());

  std::unique_ptr<gpu::SharedImageRepresentationGLTextureBase>
      gl_representation;
  if (passthrough) {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
            manager, this, tracker, std::move(texture));
  } else {
    gl_representation =
        std::make_unique<SharedImageRepresentationGLTextureVideo>(
            manager, this, tracker, std::move(texture));
  }
  return SharedImageRepresentationSkiaGL::Create(std::move(gl_representation),
                                                 std::move(context_state),
                                                 manager, this, tracker);
}

void SharedImageVideoOhosNativeImage::BeginGLReadAccess(
    const GLuint service_id) {
  stream_texture_sii_->UpdateAndBindTexImage(service_id);
}

class SharedImageVideoOhosNativeImage::SharedImageRepresentationOverlayVideo
    : public gpu::SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayVideo(
      gpu::SharedImageManager* manager,
      SharedImageVideoOhosNativeImage* backing,
      gpu::MemoryTypeTracker* tracker)
      : gpu::SharedImageRepresentationOverlay(manager, backing, tracker) {}

  SharedImageRepresentationOverlayVideo(
      const SharedImageRepresentationOverlayVideo&) = delete;
  SharedImageRepresentationOverlayVideo& operator=(
      const SharedImageRepresentationOverlayVideo&) = delete;

 protected:
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override {
    if (!stream_image()->HasTextureOwner()) {
      TRACE_EVENT0("media",
                   "SharedImageRepresentationOverlayVideo::BeginReadAccess");
      stream_image()->RenderToOverlay();
    }
    return true;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    DCHECK(release_fence.is_null());
  }

  gl::GLImage* GetGLImage() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  StreamTextureSharedImageInterface* stream_image() {
    auto* video_backing =
        static_cast<SharedImageVideoOhosNativeImage*>(backing());
    DCHECK(video_backing);
    return video_backing->stream_texture_sii_.get();
  }
};

std::unique_ptr<gpu::SharedImageRepresentationOverlay>
SharedImageVideoOhosNativeImage::ProduceOverlay(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  return std::make_unique<SharedImageRepresentationOverlayVideo>(manager, this,
                                                                 tracker);
}

}  // namespace gpu

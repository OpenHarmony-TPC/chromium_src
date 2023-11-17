// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/codec_image.h"

#include <string.h>

#include <memory>

#include "base/callback_helpers.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

CodecImage::CodecImage(const gfx::Size& coded_size,
                       scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)), coded_size_(coded_size) {}

CodecImage::~CodecImage() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  NotifyUnused();
}

void CodecImage::Initialize(
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    bool is_texture_owner_backed) {
  DCHECK(output_buffer_renderer);
  output_buffer_renderer_ = std::move(output_buffer_renderer);
  is_texture_owner_backed_ = is_texture_owner_backed;
  // promotion_hint_cb_ = std::move(promotion_hint_cb);
}

void CodecImage::AddUnusedCB(UnusedCB unused_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  unused_cbs_.push_back(std::move(unused_cb));
}

void CodecImage::NotifyUnused() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();

  ReleaseCodecBuffer();

  for (auto& cb : unused_cbs_)
    std::move(cb).Run(this);
  unused_cbs_.clear();
}

gfx::Size CodecImage::GetSize() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return coded_size_;
}

unsigned CodecImage::GetInternalFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return GL_RGBA;
}

unsigned CodecImage::GetDataType() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return GL_UNSIGNED_BYTE;
}

CodecImage::BindOrCopy CodecImage::ShouldBindOrCopy() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  return is_texture_owner_backed_ ? COPY : BIND;
}

bool CodecImage::BindTexImage(unsigned target) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  DCHECK_EQ(BIND, ShouldBindOrCopy());
  return true;
}

void CodecImage::ReleaseTexImage(unsigned target) {}

bool CodecImage::CopyTexImage(unsigned target) {
  LOG(DEBUG) << "CodecImage::CopyTexImage";
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  DCHECK(!features::NeedThreadSafeAndroidMedia());

  TRACE_EVENT0("media", "CodecImage::CopyTexImage");
  DCHECK_EQ(COPY, ShouldBindOrCopy());

  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (!output_buffer_renderer_)
    return true;

  GLint texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);

  DCHECK(output_buffer_renderer_->texture_owner()->binds_texture_on_update());
  if (texture_id > 0 &&
      static_cast<unsigned>(texture_id) !=
          output_buffer_renderer_->texture_owner()->GetTextureId()) {
    return false;
  }
  output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(
      BindingsMode::kBindImage,
      output_buffer_renderer_->texture_owner()->GetTextureId());
  return true;
}

bool CodecImage::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  return false;
}

void CodecImage::NotifyOverlayPromotion(bool promotion,
                                        const gfx::Rect& bounds) {}

void CodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {}

void CodecImage::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  ReleaseCodecBuffer();
}

bool CodecImage::IsUsingGpuMemory() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;

  return output_buffer_renderer_->was_tex_image_bound();
}

void CodecImage::UpdateAndBindTexImage(GLuint service_id) {
  AssertAcquiredDrDcLock();
  RenderToTextureOwnerFrontBuffer(BindingsMode::kBindImage, service_id);
}

bool CodecImage::HasTextureOwner() const {
  return !!texture_owner();
}

gpu::TextureBase* CodecImage::GetTextureBase() const {
  return texture_owner()->GetTextureBase();
}

bool CodecImage::RenderToFrontBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToFrontBuffer();
}

bool CodecImage::RenderToTextureOwnerBackBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;

  return output_buffer_renderer_->RenderToTextureOwnerBackBuffer();
}

bool CodecImage::RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode,
                                                 GLuint service_id) {
  LOG(DEBUG) << "CodecImage::RenderToTextureOwnerFrontBuffer";
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(bindings_mode,
                                                                  service_id);
}

bool CodecImage::RenderToOverlay() {
  return false;
}

bool CodecImage::TextureOwnerBindsTextureOnUpdate() {
  AssertAcquiredDrDcLock();
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->texture_owner()->binds_texture_on_update();
}

void CodecImage::ReleaseCodecBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  output_buffer_renderer_.reset();
}

bool CodecImage::HasMutableState() const {
  return false;
}

CodecImageHolder::CodecImageHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecImage> codec_image)
    : base::RefCountedDeleteOnSequence<CodecImageHolder>(
          std::move(task_runner)),
      codec_image_(std::move(codec_image)) {}

CodecImageHolder::~CodecImageHolder() = default;

}  // namespace media

// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ohos/native_image_texture_owner.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/abstract_texture_impl.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_gl_owner.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

NativeImageTextureOwner::NativeImageTextureOwner(
    bool binds_texture_on_update,
    std::unique_ptr<gles2::AbstractTexture> texture,
    scoped_refptr<SharedContextState> context_state)
    : base::RefCountedDeleteOnSequence<NativeImageTextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      binds_texture_on_update_(binds_texture_on_update),
      context_state_(std::move(context_state)),
      texture_(std::move(texture)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(context_state_);
  context_state_->AddContextLostObserver(this);
}

NativeImageTextureOwner::NativeImageTextureOwner(
    bool binds_texture_on_update,
    std::unique_ptr<gles2::AbstractTexture> texture)
    : base::RefCountedDeleteOnSequence<NativeImageTextureOwner>(
          base::ThreadTaskRunnerHandle::Get()),
      binds_texture_on_update_(binds_texture_on_update),
      texture_(std::move(texture)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

NativeImageTextureOwner::~NativeImageTextureOwner() {
  bool have_context = true;
  absl::optional<ui::ScopedMakeCurrent> scoped_make_current;
  if (!context_state_) {
    have_context = false;
  } else {
    if (!context_state_->IsCurrent(nullptr, /*needs_gl=*/true)) {
      scoped_make_current.emplace(context_state_->context(),
                                  context_state_->surface());
      have_context = scoped_make_current->IsContextCurrent();
    }
    context_state_->RemoveContextLostObserver(this);
  }
  if (!have_context) {
    texture_->NotifyOnContextLost();
  }

  texture_.reset();
  context_state_.reset();
}

// static
scoped_refptr<NativeImageTextureOwner> NativeImageTextureOwner::Create(
    std::unique_ptr<gles2::AbstractTexture> texture,
    Mode mode,
    scoped_refptr<SharedContextState> context_state) {
  return new NativeImageTextureGlOwner(std::move(texture),
                                       std::move(context_state));
}

// static
std::unique_ptr<gles2::AbstractTexture> NativeImageTextureOwner::CreateTexture(
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(context_state);

  gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    return std::make_unique<gles2::AbstractTextureImplPassthrough>(
        GL_TEXTURE_EXTERNAL_OES, GL_RGBA,
        0,  // width
        0,  // height
        1,  // depth
        0,  // border
        GL_RGBA, GL_UNSIGNED_BYTE);
  }

  return std::make_unique<gles2::AbstractTextureImpl>(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA,
      0,  // width
      0,  // height
      1,  // depth
      0,  // border
      GL_RGBA, GL_UNSIGNED_BYTE);
}

GLuint NativeImageTextureOwner::GetTextureId() const {
  return texture_->service_id();
}

TextureBase* NativeImageTextureOwner::GetTextureBase() const {
  return texture_->GetTextureBase();
}

void NativeImageTextureOwner::OnContextLost() {
  ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_.reset();
}

}  // namespace gpu
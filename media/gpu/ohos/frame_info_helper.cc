// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ohos/frame_info_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/ohos/codec_output_buffer_renderer.h"

namespace media {

FrameInfoHelper::FrameInfo::FrameInfo() = default;
FrameInfoHelper::FrameInfo::~FrameInfo() = default;
FrameInfoHelper::FrameInfo::FrameInfo(FrameInfo&&) = default;
FrameInfoHelper::FrameInfo::FrameInfo(const FrameInfoHelper::FrameInfo&) =
    default;
FrameInfoHelper::FrameInfo& FrameInfoHelper::FrameInfo::operator=(
    const FrameInfoHelper::FrameInfo&) = default;

class FrameInfoHelperImpl : public FrameInfoHelper,
                            public gpu::RefCountedLockHelperDrDc {
 public:
  FrameInfoHelperImpl(scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
                      SharedImageVideoProvider::GetStubCB get_stub_cb,
                      scoped_refptr<gpu::RefCountedLock> drdc_lock)
      : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)) {
    on_gpu_ = base::SequenceBound<OnGpu>(std::move(gpu_task_runner),
                                         std::move(get_stub_cb));
  }

  ~FrameInfoHelperImpl() override = default;

  void GetFrameInfo(std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
                    FrameInfoReadyCB callback) override {
    Request request = {.buffer_renderer = std::move(buffer_renderer),
                       .callback = std::move(callback)};
    requests_.push(std::move(request));
    if (requests_.size() == 1)
      ProcessRequestsQueue();
  }

 private:
  struct Request {
    std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer;
    FrameInfoReadyCB callback;
  };

  class OnGpu : public gpu::CommandBufferStub::DestructionObserver {
   public:
    OnGpu(SharedImageVideoProvider::GetStubCB get_stub_cb) {
      stub_ = get_stub_cb.Run();
      if (stub_)
        stub_->AddDestructionObserver(this);
    }

    ~OnGpu() override {
      if (stub_)
        stub_->RemoveDestructionObserver(this);
    }

    void OnWillDestroyStub(bool have_context) override {
      LOG(ERROR) << "OnGpu::OnWillDestroyStub ";
      DCHECK(stub_);
      stub_ = nullptr;
    }

    void GetFrameInfoImpl(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb,
        std::unique_ptr<base::AutoLockMaybe> scoped_drdc_lock) {
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      absl::optional<FrameInfo> info;

      if (buffer_renderer->RenderToTextureOwnerFrontBuffer(
              CodecOutputBufferRenderer::BindingsMode::kDontBindImage, 0)) {
        gfx::Size coded_size;
        gfx::Rect visible_rect;
        if (texture_owner->GetCodedSizeAndVisibleRect(
                buffer_renderer->size(), &coded_size, &visible_rect)) {
          info.emplace();
          info->coded_size = coded_size;
          info->visible_rect = visible_rect;
        }
      }

      scoped_drdc_lock.reset();
      std::move(cb).Run(std::move(buffer_renderer), info);
    }

    void GetFrameInfo(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb,
        scoped_refptr<gpu::RefCountedLock> drdc_lock) {
      auto scoped_drdc_lock = std::make_unique<base::AutoLockMaybe>(
          drdc_lock ? drdc_lock->GetDrDcLockPtr() : nullptr);

      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      auto buffer_available_cb =
          base::BindOnce(&OnGpu::GetFrameInfoImpl, weak_factory_.GetWeakPtr(),
                         std::move(buffer_renderer), std::move(cb),
                         std::move(scoped_drdc_lock));
      texture_owner->RunWhenBufferIsAvailable(std::move(buffer_available_cb));
    }

   private:
    raw_ptr<gpu::CommandBufferStub> stub_ = nullptr;
    base::WeakPtrFactory<OnGpu> weak_factory_{this};
  };

  FrameInfo GetFrameInfoWithVisibleSize(const gfx::Size& visible_size) {
    FrameInfo info;
    info.coded_size = visible_size;
    info.visible_rect = gfx::Rect(visible_size);
    return info;
  }

  void OnFrameInfoReady(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      absl::optional<FrameInfo> frame_info) {
    DCHECK(buffer_renderer);
    DCHECK(!requests_.empty());

    auto& request = requests_.front();

    if (frame_info) {
      visible_size_ = buffer_renderer->size();
      frame_info_ = *frame_info;
      std::move(request.callback).Run(std::move(buffer_renderer), frame_info_);
    } else {
      auto info = GetFrameInfoWithVisibleSize(buffer_renderer->size());
      std::move(request.callback)
          .Run(std::move(buffer_renderer), std::move(info));
    }
    requests_.pop();
    ProcessRequestsQueue();
  }

  void ProcessRequestsQueue() {
    while (!requests_.empty()) {
      auto& request = requests_.front();
      if (!request.buffer_renderer) {
        std::move(request.callback).Run(nullptr, FrameInfo());
      } else if (!request.buffer_renderer->texture_owner()) {
        auto info =
            GetFrameInfoWithVisibleSize(request.buffer_renderer->size());
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), std::move(info));
      } else if (visible_size_ == request.buffer_renderer->size()) {
        LOG(DEBUG) << "OnGpu::ProcessRequestsQueue same size";
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), frame_info_);
      } else {
        LOG(DEBUG) << "OnGpu::ProcessRequestsQueue will GetFrameInfo";
        auto cb = BindToCurrentLoop(
            base::BindOnce(&FrameInfoHelperImpl::OnFrameInfoReady,
                           weak_factory_.GetWeakPtr()));

        on_gpu_.AsyncCall(&OnGpu::GetFrameInfo)
            .WithArgs(std::move(request.buffer_renderer), std::move(cb),
                      GetDrDcLock());
        break;
      }
      requests_.pop();
    }
  }

  base::SequenceBound<OnGpu> on_gpu_;
  std::queue<Request> requests_;

  // Cached values.
  FrameInfo frame_info_;
  gfx::Size visible_size_;

  base::WeakPtrFactory<FrameInfoHelperImpl> weak_factory_{this};
};

// static
std::unique_ptr<FrameInfoHelper> FrameInfoHelper::Create(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::GetStubCB get_stub_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  return std::make_unique<FrameInfoHelperImpl>(
      std::move(gpu_task_runner), std::move(get_stub_cb), std::move(drdc_lock));
}

}  // namespace media

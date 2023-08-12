// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_ohos.h"

#include <surface.h>
#include <sys/time.h>
#include <window.h>
#include <iomanip>
#include <thread>
#include "nweb_native_window_tracker.h"

namespace {
static int64_t GetNowTime() {
  struct timeval start = {};
  gettimeofday(&start, nullptr);
  constexpr uint32_t kSecToUsec = 1000 * 1000;
  return static_cast<int64_t>(start.tv_sec) * kSecToUsec + start.tv_usec;
}
}  // namespace

namespace gl {

scoped_refptr<gl::NativeViewGLSurfaceEGLOhos>
NativeViewGLSurfaceEGLOhos::CreateNativeViewGLSurfaceEGLOhos(
    gfx::AcceleratedWidget widget) {
  NativeWindow* window =
      (NativeWindow*)NWebNativeWindowTracker::Instance().GetNativeWindow(
          widget);
  return scoped_refptr<NativeViewGLSurfaceEGLOhos>(
      new NativeViewGLSurfaceEGLOhos(EGLNativeWindowType(window)));
}

NativeViewGLSurfaceEGLOhos::NativeViewGLSurfaceEGLOhos(
    EGLNativeWindowType window)
    : NativeViewGLSurfaceEGL(window, nullptr) {
  frame_counter_.reset(new FrameCounter);
  if (frame_counter_) {
    frame_counter_->Start();
  }
}

gfx::SwapResult NativeViewGLSurfaceEGLOhos::SwapBuffers(
    PresentationCallback callback) {
  auto result = NativeViewGLSurfaceEGL::SwapBuffers(std::move(callback));
  if (frame_counter_) {
    frame_counter_->Update(result == gfx::SwapResult::SWAP_FAILED ? false
                                                                  : true);
  }
  return result;
}

void NativeViewGLSurfaceEGLOhos::FrameCounter::Start() {
  std::weak_ptr<FrameCounter> frame_counter_weak(shared_from_this());
  std::thread frame_stat_thread([frame_counter_weak]() {
    while (!frame_counter_weak.expired()) {
      {
        auto frame_counter = frame_counter_weak.lock();
        std::unique_lock<std::mutex> lk(frame_counter->frame_stat_mtx_);
        int64_t curr_time = GetNowTime();
        if (frame_counter->last_time_ == 0) {
          frame_counter->local_render_count_ = 0;
          frame_counter->last_time_ = curr_time;
          continue;
        }

        frame_counter->local_render_count_ = 0;
        frame_counter->last_time_ = curr_time;
      }
      // during sleep(), frame_counter_weak should not be
      // promote to shared_ptr, to avoid add refcount
      constexpr int kFrameStatUpdateDur = 1;
      sleep(kFrameStatUpdateDur);
    }
  });
  frame_stat_thread.detach();
}

void NativeViewGLSurfaceEGLOhos::FrameCounter::Update(bool flag) {
  std::unique_lock<std::mutex> lk(frame_stat_mtx_);
  if (flag) {
    frame_render_count_++;
    local_render_count_++;
  } else {
    frame_miss_count_++;
  }
}

}  // namespace gl

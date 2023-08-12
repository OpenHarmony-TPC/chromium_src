// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_OHOS_H_
#define UI_GL_GL_SURFACE_EGL_OHOS_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_bindings.h"

namespace gl {
typedef struct WindowsSurfaceInfoTag {
  void* window;
  EGLDisplay display;
  EGLContext context;
  EGLSurface surface;
} WindowsSurfaceInfo;

class GL_EXPORT NativeViewGLSurfaceEGLOhos : public NativeViewGLSurfaceEGL {
 public:
  explicit NativeViewGLSurfaceEGLOhos(EGLNativeWindowType window);
  NativeViewGLSurfaceEGLOhos(const NativeViewGLSurfaceEGLOhos& other) = delete;
  NativeViewGLSurfaceEGLOhos& operator=(const NativeViewGLSurfaceEGLOhos& rhs) =
      delete;

  static scoped_refptr<gl::NativeViewGLSurfaceEGLOhos>
  CreateNativeViewGLSurfaceEGLOhos(gfx::AcceleratedWidget window);
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;

 private:
  class FrameCounter : public std::enable_shared_from_this<FrameCounter> {
   public:
    void Start();
    void Update(bool flag);

   private:
    std::mutex frame_stat_mtx_;
    int64_t frame_render_count_ = 0L;
    int64_t frame_miss_count_ = 0L;
    int64_t local_render_count_ = 0L;
    int64_t last_time_ = 0L;
  };
  std::shared_ptr<FrameCounter> frame_counter_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_OHOS_H_

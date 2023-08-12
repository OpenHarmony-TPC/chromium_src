/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NWEB_RENDER_HANDLER_H
#define NWEB_RENDER_HANDLER_H

#include <functional>
#include <memory>
#include <vector>
#include "cef/include/cef_render_handler.h"
#include "display_manager_adapter.h"
#include "nweb_handler.h"
#include "nweb_inputmethod_client.h"
#include "nweb_touch_handle_state_impl.h"

namespace OHOS::NWeb {
struct NWebScreenInfo {
  RotationType rotation = RotationType::ROTATION_0;
  OrientationType orientation = OrientationType::UNSPECIFIED;
  int width = 0;
  int height = 0;
  double display_ratio = -1.0;
  bool default_portrait = false;
};

class NWebRenderHandler : public CefRenderHandler {
 public:
  static CefRefPtr<NWebRenderHandler> Create();
  NWebRenderHandler() = default;
  ~NWebRenderHandler() = default;

  void RegisterRenderCb(std::function<void(const char*)> render_update_cb);
  void RegisterNWebHandler(std::shared_ptr<NWebHandler> handler);
  void Resize(uint32_t width, uint32_t height);
  void SetScreenInfo(const NWebScreenInfo& screen_info);
  int ContentHeight();
  void SetInputMethodClient(CefRefPtr<NWebInputMethodClient> client);

  /* CefRenderHandler method begin */
  virtual void GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) override;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override;
  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirty_rects,
                       const void* buffer,
                       int width,
                       int height) override;

  void OnRootLayerChanged(CefRefPtr<CefBrowser> browser,
                          int height,
                          int width) override;

  void OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser,
                             double x,
                             double y) override;

  virtual void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selected_range,
      const RectList& character_bounds) override;

  virtual void OnTextSelectionChanged(CefRefPtr<CefBrowser> browser,
                                      const CefString& selected_text,
                                      const CefRange& selected_range) override;

  virtual void OnVirtualKeyboardRequested(CefRefPtr<CefBrowser> browser,
                                          TextInputMode input_mode,
                                          bool show_keyboard) override;

  void GetTouchHandleSize(CefRefPtr<CefBrowser> browser,
                          cef_horizontal_alignment_t orientation,
                          CefSize& size) override;
  void OnTouchSelectionChanged(
      const CefTouchHandleState& insert_handle,
      const CefTouchHandleState& start_selection_handle,
      const CefTouchHandleState& end_selection_handle,
      bool need_report) override;

  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     DragOperationsMask allowed_ops,
                     int x,
                     int y) override;
  /* CefRenderHandler method end */

  std::shared_ptr<NWebTouchHandleState> GetTouchHandleState(
      NWebTouchHandleState::TouchHandleType type);

  CefRefPtr<CefDragData> GetDragData();

  float GetVirtualPixelRatio() const { return screen_info_.display_ratio; }

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(NWebRenderHandler);

 private:
  CefTouchHandleState ConvertTouchHandleDisplayRatio(
      const CefTouchHandleState& touch_handle);

  std::function<void(const char*)> render_update_cb_ = nullptr;
  CefRefPtr<NWebInputMethodClient> inputmethod_client_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  int content_height_ = 0;
  int content_width_ = 0;
  NWebScreenInfo screen_info_;

  std::weak_ptr<NWebHandler> handler_;
  CefTouchHandleState insert_handle_;
  CefTouchHandleState start_selection_handle_;
  CefTouchHandleState end_selection_handle_;
  CefRefPtr<CefDragData> drag_data_ = nullptr;
};
}  // namespace OHOS::NWeb

#endif  // NWEB_RENDER_HANDLER_H

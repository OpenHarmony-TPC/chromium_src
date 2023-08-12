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

#include "nweb_render_handler.h"

#include <sys/time.h>
#include <cerrno>
#include <cstring>

#include "base/logging.h"
#include "nweb_touch_handle_state_impl.h"

#include "ohos_adapter_helper.h"

namespace {
cef_screen_orientation_type_t ConvertOrientationType(
    OHOS::NWeb::OrientationType type,
    bool default_portrait) {
  switch (type) {
    case OHOS::NWeb::OrientationType::UNSPECIFIED:
      return default_portrait
                 ? cef_screen_orientation_type_t::PORTRAIT_PRIMARY
                 : cef_screen_orientation_type_t::LANDSCAPE_PRIMARY;
    case OHOS::NWeb::OrientationType::VERTICAL:
      return cef_screen_orientation_type_t::PORTRAIT_PRIMARY;
    case OHOS::NWeb::OrientationType::HORIZONTAL:
      return default_portrait
                 ? cef_screen_orientation_type_t::LANDSCAPE_SECONDARY
                 : cef_screen_orientation_type_t::LANDSCAPE_PRIMARY;
    case OHOS::NWeb::OrientationType::REVERSE_VERTICAL:
      return cef_screen_orientation_type_t::PORTRAIT_SECONDARY;
    case OHOS::NWeb::OrientationType::REVERSE_HORIZONTAL:
      return default_portrait
                 ? cef_screen_orientation_type_t::LANDSCAPE_PRIMARY
                 : cef_screen_orientation_type_t::LANDSCAPE_SECONDARY;
    // Now ohos platform don't hava sensor orientation.
    // Will be support later.
    case OHOS::NWeb::OrientationType::SENSOR:
    case OHOS::NWeb::OrientationType::SENSOR_VERTICAL:
    case OHOS::NWeb::OrientationType::SENSOR_HORIZONTAL:
      return cef_screen_orientation_type_t::UNDEFINED;
    default:
      return cef_screen_orientation_type_t::UNDEFINED;
  }
}

uint16_t ConvertRotationAngel(OHOS::NWeb::RotationType type,
                              bool default_portrait) {
  // Notice: 90 and 270 is reverse.
  switch (type) {
    case OHOS::NWeb::RotationType::ROTATION_0:
      return default_portrait ? 0 : 90;
    case OHOS::NWeb::RotationType::ROTATION_90:
      return default_portrait ? 270 : 0;
    case OHOS::NWeb::RotationType::ROTATION_180:
      return default_portrait ? 180 : 270;
    case OHOS::NWeb::RotationType::ROTATION_270:
      return default_portrait ? 90 : 180;
    default:
      return 0;
  }
}
}  // namespace

namespace OHOS::NWeb {
// static
CefRefPtr<NWebRenderHandler> NWebRenderHandler::Create() {
  CefRefPtr<NWebRenderHandler> renderHandler(new NWebRenderHandler());
  if (renderHandler == nullptr) {
    LOG(ERROR) << "fail to create NWebRenderHandler instance";
    return nullptr;
  }
  return renderHandler;
}

void NWebRenderHandler::RegisterRenderCb(
    std::function<void(const char*)> render_update_cb) {
  render_update_cb_ = render_update_cb;
}

void NWebRenderHandler::RegisterNWebHandler(
    std::shared_ptr<NWebHandler> handler) {
  handler_ = std::weak_ptr<NWebHandler>(handler);
}

void NWebRenderHandler::SetInputMethodClient(
    CefRefPtr<NWebInputMethodClient> client) {
  inputmethod_client_ = client;
}

void NWebRenderHandler::Resize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
}

void NWebRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser,
                                    CefRect& rect) {
  rect.x = 0;
  rect.y = 0;
  if (screen_info_.display_ratio <= 0) {
    rect.width = width_;
    rect.height = height_;
  } else {
    rect.width = width_ / screen_info_.display_ratio;
    rect.height = height_ / screen_info_.display_ratio;
  }

  if (rect.width <= 0) {
    rect.width = 1;
  }
  if (rect.height <= 0) {
    rect.height = 1;
  }
}

void NWebRenderHandler::SetScreenInfo(const NWebScreenInfo& screen_info) {
  screen_info_ = screen_info;
}

bool NWebRenderHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                      CefScreenInfo& screen_info) {
  screen_info.orientation = ConvertOrientationType(
      screen_info_.orientation, screen_info_.default_portrait);
  screen_info.angle = ConvertRotationAngel(screen_info_.rotation,
                                           screen_info_.default_portrait);
  screen_info.rect.width = screen_info_.width;
  screen_info.rect.height = screen_info_.height;
  screen_info.device_scale_factor = screen_info_.display_ratio;

  // TODO: currently display dont have interface to get. We use fix value
  // instead.
  screen_info.depth = 24;
  screen_info.depth_per_component = 8;

  cef_device_ratio_ = screen_info.device_scale_factor;
  return true;
}

void NWebRenderHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                                PaintElementType type,
                                const RectList& dirty_rects,
                                const void* buffer,
                                int width,
                                int height) {
  // run render callback to take buffer.
  // note: buffer must be consumed asap with no lock protected
  if (render_update_cb_ == nullptr) {
    return;
  }
  if (width != width_ || height != height_) {
    LOG(INFO) << "frame size(" << width << "*" << height
              << ") is not identical to request size (" << width_ << "*"
              << height_ << "), drop this frame";
    constexpr uint8_t kBitsPerPixel = 4;
    uint32_t white_frame_size = width_ * height_ * kBitsPerPixel;
    char* white_frame = new char[white_frame_size];
    const char pixel_in_white = 0xFF;
    (void)memset(white_frame, pixel_in_white, white_frame_size);
    render_update_cb_(white_frame);
    delete[] white_frame;
  } else {
    render_update_cb_(reinterpret_cast<const char*>(buffer));
  }
}

void NWebRenderHandler::OnRootLayerChanged(CefRefPtr<CefBrowser> browser,
                                           int height,
                                           int width) {
  content_height_ = height;
  content_width_ = width;
}

void NWebRenderHandler::OnScrollOffsetChanged(CefRefPtr<CefBrowser> browser,
                                              double x,
                                              double y) {
  if (auto handler = handler_.lock()) {
    handler->OnScroll(x, y);
  }
}

int NWebRenderHandler::ContentHeight() {
  return content_height_;
}

void NWebRenderHandler::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selected_range,
    const RectList& character_bounds) {
  LOG(INFO) << "NWebRenderHandler::OnImeCompositionRangeChanged";
}

void NWebRenderHandler::OnTextSelectionChanged(CefRefPtr<CefBrowser> browser,
                                               const CefString& selected_text,
                                               const CefRange& selected_range) {
  if (inputmethod_client_) {
    inputmethod_client_->OnTextSelectionChanged(browser, selected_text,
                                                selected_range);
  }
}

void NWebRenderHandler::OnVirtualKeyboardRequested(
    CefRefPtr<CefBrowser> browser,
    TextInputMode input_mode,
    bool show_keyboard) {
  LOG(INFO) << "NWebRenderHandler::OnVirtualKeyboardRequested input_mode = "
            << input_mode << ", show_keyboard = " << show_keyboard;

  if (inputmethod_client_) {
    if (input_mode != CEF_TEXT_INPUT_MODE_NONE) {
      inputmethod_client_->Attach(browser, show_keyboard, input_mode);
    } else {
      inputmethod_client_->HideTextInput();
    }
  }
}

void NWebRenderHandler::GetTouchHandleSize(
    CefRefPtr<CefBrowser> browser,
    cef_horizontal_alignment_t orientation,
    CefSize& size) {
  // TODO: need to refactor in 3.2.8.1 use arkui refactor.
  size.width = 10;
  size.height = 10;
  if (screen_info_.display_ratio <= 0.0) {
    LOG(ERROR) << "invalid display_ratio_, display_ratio_ = "
               << screen_info_.display_ratio;
    return;
  }
  if (screen_info_.display_ratio <= 1) {
    return;
  } else if (screen_info_.display_ratio > 1 &&
             screen_info_.display_ratio < 1.7) {
    // rk
    size.width = 30 / screen_info_.display_ratio;
    size.height = 30 / screen_info_.display_ratio;
  } else if (screen_info_.display_ratio >= 1.7 &&
             screen_info_.display_ratio < 2.5) {
    // wgr
    size.width = 40 / screen_info_.display_ratio;
    size.height = 40 / screen_info_.display_ratio;
  } else {
    // phone
    size.width = 60 / screen_info_.display_ratio;
    size.height = 60 / screen_info_.display_ratio;
  }
  LOG(INFO) << "GetTouchHandleSize " << size.width << " " << size.height;
}

std::shared_ptr<NWebTouchHandleState> NWebRenderHandler::GetTouchHandleState(
    NWebTouchHandleState::TouchHandleType type) {
  switch (type) {
    case NWebTouchHandleState::TouchHandleType::INSERT_HANDLE:
      return insert_handle_.enabled
                 ? std::make_shared<NWebTouchHandleStateImpl>(insert_handle_)
                 : nullptr;
    case NWebTouchHandleState::TouchHandleType::SELECTION_BEGIN_HANDLE:
      return start_selection_handle_.enabled
                 ? std::make_shared<NWebTouchHandleStateImpl>(
                       start_selection_handle_)
                 : nullptr;
    case NWebTouchHandleState::TouchHandleType::SELECTION_END_HANDLE:
      return end_selection_handle_.enabled
                 ? std::make_shared<NWebTouchHandleStateImpl>(
                       end_selection_handle_)
                 : nullptr;
    default:
      return nullptr;
  }
}

CefTouchHandleState NWebRenderHandler::ConvertTouchHandleDisplayRatio(
    const CefTouchHandleState& touch_handle) {
  CefTouchHandleState result_touch_handle = touch_handle;
  if (screen_info_.display_ratio <= 0) {
    LOG(WARNING) << "virtual display ratio is invalid";
    return result_touch_handle;
  }
  result_touch_handle.edge_height *= screen_info_.display_ratio;
  result_touch_handle.origin.x *= screen_info_.display_ratio;
  result_touch_handle.origin.y *= screen_info_.display_ratio;
  return result_touch_handle;
}

void NWebRenderHandler::OnTouchSelectionChanged(
    const CefTouchHandleState& insert_handle,
    const CefTouchHandleState& start_selection_handle,
    const CefTouchHandleState& end_selection_handle,
    bool need_report) {
  insert_handle_ = ConvertTouchHandleDisplayRatio(insert_handle);
  start_selection_handle_ = ConvertTouchHandleDisplayRatio(start_selection_handle);
  end_selection_handle_ = ConvertTouchHandleDisplayRatio(end_selection_handle);
  if (!need_report) {
    return;
  }
  if (auto handler = handler_.lock()) {
    handler->OnTouchSelectionChanged(
        std::make_shared<NWebTouchHandleStateImpl>(insert_handle_),
        std::make_shared<NWebTouchHandleStateImpl>(start_selection_handle_),
        std::make_shared<NWebTouchHandleStateImpl>(end_selection_handle_));
  }
}

bool NWebRenderHandler::StartDragging(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefDragData> drag_data,
                                      DragOperationsMask allowed_ops,
                                      int x,
                                      int y) {
  LOG(INFO) << "received start dragging callback, operation = " << allowed_ops
            << ", x = " << x << ", y = " << y;
  if (!drag_data && !drag_data->HasImage()) {
    LOG(ERROR) << "drag data invalid";
    return false;
  }

  auto image = drag_data->GetImage();
  if (!image) {
    LOG(ERROR) << "drag data image invalid";
    return false;
  }

  int width;
  int height;
  auto bitmap = image->GetAsBitmap(1, CEF_COLOR_TYPE_BGRA_8888,
                                   CEF_ALPHA_TYPE_OPAQUE, width, height);
  if (!bitmap) {
    LOG(ERROR) << "drag data bitmap invalid";
    return false;
  }

  size_t data_size = bitmap->GetSize();
  void* buffer = calloc(1, data_size);
  if (!buffer) {
    LOG(ERROR) << "calloc failed";
    return false;
  }
  size_t read_size = bitmap->GetData(buffer, data_size, 0);
  if (read_size != data_size) {
    free(buffer);
    LOG(ERROR) << "get data from bitmap failed";
    return false;
  }

  LOG(INFO) << "drag image width : " << width << ", height : " << height
            << ", buffer size : " << read_size;
  auto handler = handler_.lock();
  if (handler == nullptr) {
    LOG(ERROR) << "can't get strong ptr with handler";
    free(buffer);
    return false;
  }

  ImageOptions opt;
  opt.colorType = ImageColorType::COLOR_TYPE_BGRA_8888;
  opt.alphaType = ImageAlphaType::ALPHA_TYPE_OPAQUE;
  opt.width = width;
  opt.height = height;
  bool isNeedDrag = handler->OnDragAndDropData(buffer, read_size, opt);
  if (isNeedDrag) {
    drag_data_ = drag_data;
  }
  free(buffer);
  return isNeedDrag;
}

CefRefPtr<CefDragData> NWebRenderHandler::GetDragData() {
  return drag_data_;
}
}  // namespace OHOS::NWeb

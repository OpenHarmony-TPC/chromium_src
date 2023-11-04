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

#include "nweb_event_handler.h"

#include "cef/include/base/cef_logging.h"
#include "cef/include/base/cef_macros.h"
#include "cef/include/internal/cef_types.h"
#include "cef/include/internal/cef_types_wrappers.h"

#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keysym_to_unicode.h"

namespace OHOS::NWeb {

constexpr double MAX_ZOOM_FACTOR = 10.0;
constexpr double ZOOM_FACTOR = 2.0;

// static
std::shared_ptr<NWebEventHandler> NWebEventHandler::Create(bool is_enhance_surface) {
  auto event_handler = std::make_shared<NWebEventHandler>(is_enhance_surface);
  if (event_handler == nullptr) {
    LOG(ERROR) << "fail to create NWebEventHandler instance";
    return nullptr;
  }
  return event_handler;
}

NWebEventHandler::NWebEventHandler(bool is_enhance_surface) {
  if (is_enhance_surface) {
    LOG(ERROR) << "MMIAdapter is not support.";
    return;
  }
  mmi_adapter_ = OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateMMIAdapter();
  if (mmi_adapter_ == nullptr) {
    LOG(ERROR) << "display_manager_adapter is nullptr";
    return;
  }
}

void NWebEventHandler::OnDestroy() {
  if (mmi_id_ > 0 && mmi_adapter_ != nullptr) {
    mmi_adapter_->UnregisterMMIInputListener(mmi_id_);
  }
  browser_ = nullptr;
}

void NWebEventHandler::SetBrowser(CefRefPtr<CefBrowser> browser) {
  browser_ = browser;
}

void NWebEventHandler::OnTouchPress(int32_t id, double x, double y) {
  CefTouchEvent touch_pressed;
  touch_pressed.type = CEF_TET_PRESSED;
  touch_pressed.pointer_type = CEF_POINTER_TYPE_TOUCH;
  touch_pressed.id = id;
  touch_pressed.x = x;
  touch_pressed.y = y;
  touch_pressed.modifiers = EVENTFLAG_NONE;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendTouchEvent(touch_pressed);
  }
}

void NWebEventHandler::OnTouchMove(int32_t id, double x, double y) {
  CefTouchEvent touch_move;
  touch_move.type = CEF_TET_MOVED;
  touch_move.pointer_type = CEF_POINTER_TYPE_TOUCH;
  touch_move.id = id;
  touch_move.x = x;
  touch_move.y = y;
  touch_move.modifiers = EVENTFLAG_NONE;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendTouchEvent(touch_move);
  }
}

void NWebEventHandler::OnTouchRelease(int32_t id, double x, double y) {
  CefTouchEvent touch_end;
  touch_end.type = CEF_TET_RELEASED;
  touch_end.pointer_type = CEF_POINTER_TYPE_TOUCH;
  touch_end.id = id;
  touch_end.x = x;
  touch_end.y = y;
  touch_end.modifiers = EVENTFLAG_NONE;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendTouchEvent(touch_end);
  }
}

void NWebEventHandler::OnTouchCancel() {
  CefTouchEvent touch_cancelled;
  touch_cancelled.type = CEF_TET_CANCELLED;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendTouchEvent(touch_cancelled);
  }
}

void NWebEventHandler::SendKeyEventFromMMI(int32_t keyCode, int32_t keyAction) {
  if (!isFocus_ || !NWebInputDelegate::IsMMIKeyEvent(keyCode)) {
    return;
  }
  LOG(DEBUG) << "SendKeyEventFromMMI keyCode = " << keyCode << " keyAction = " << keyAction;
  SendKeyEvent(keyCode, keyAction);
}

bool NWebEventHandler::SendKeyEventFromAce(int32_t keyCode, int32_t keyAction) {
  if (mmi_id_ >= 0 && NWebInputDelegate::IsMMIKeyEvent(keyCode)) {
    return true;
  }
  LOG(DEBUG) << "SendKeyEventFromAce keyCode = " << keyCode << " keyAction = " << keyAction;
  return SendKeyEvent(keyCode, keyAction);
}

bool NWebEventHandler::SendKeyEvent(int32_t keyCode, int32_t keyAction) {
  LOG(DEBUG) << "SendKeyEvent keyCode = " << keyCode << " keyAction = " << keyAction;
  CefKeyEvent keyEvent;
  input_delegate_.SetModifiers(keyCode, keyAction);
  keyEvent.windows_key_code =
      NWebInputDelegate::CefConverter("keycode", keyCode);
  keyEvent.type = static_cast<cef_key_event_type_t>(
      NWebInputDelegate::CefConverter("keyaction", keyAction));
  keyEvent.modifiers = input_delegate_.GetModifiers();
  keyEvent.is_system_key = false;
  keyEvent.native_key_code =
      NWebInputDelegate::CefConverter("keyscancode", keyCode);
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(keyEvent.windows_key_code);
  int keysym = ui::XKeysymForWindowsKeyCode(
      key_code, keyEvent.modifiers & EVENTFLAG_SHIFT_DOWN);
  char16_t character = ui::GetUnicodeCharacterFromXKeySym(keysym);
  keyEvent.character = keyEvent.unmodified_character = character;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }

  if (keyEvent.type == KEYEVENT_RAWKEYDOWN) {
    keyEvent.type = KEYEVENT_CHAR;
    if (browser_ && browser_->GetHost()) {
      browser_->GetHost()->SendKeyEvent(keyEvent);
    }
  }

  return true;
}

void NWebEventHandler::SendMouseWheelEvent(double x,
                                           double y,
                                           double deltaX,
                                           double deltaY) {
  CefMouseEvent mouseEvent;
  mouseEvent.x = x;
  mouseEvent.y = y;
  mouseEvent.modifiers = input_delegate_.GetModifiers();
  if (!browser_ || !browser_->GetHost()){
    return;
  }
  if ((mouseEvent.modifiers & EVENTFLAG_CONTROL_DOWN) && (deltaY != 0)) {
    double curFactor = browser_->GetHost()->GetZoomLevel();
    double tempZoomFactor = deltaY < 0 ? curFactor + ZOOM_FACTOR : curFactor - ZOOM_FACTOR;
    if (tempZoomFactor > MAX_ZOOM_FACTOR || tempZoomFactor < 0) {
      LOG(ERROR) << "The mouse wheel event can no longer be zoomed in or out.";
      return;
    }
    browser_->GetHost()->SetZoomLevel(tempZoomFactor);
    return;
  }
  double horizontalDelta;
  double verticalDelta;
  if (mmi_id_ > 0 && (mouseEvent.modifiers & EVENTFLAG_SHIFT_DOWN)) {
    horizontalDelta = deltaY * input_delegate_.GetMouseWheelRatio();
    verticalDelta = deltaX * input_delegate_.GetMouseWheelRatio();
  } else {
    horizontalDelta = deltaX * input_delegate_.GetMouseWheelRatio();
    verticalDelta = deltaY * input_delegate_.GetMouseWheelRatio();
  }
  browser_->GetHost()->SendMouseWheelEvent(mouseEvent, horizontalDelta, verticalDelta);
}

void NWebEventHandler::SendMouseEvent(int x, int y, int button, int action, int count) {
  CefMouseEvent mouseEvent;
  mouseEvent.x = x;
  mouseEvent.y = y;
  cef_mouse_button_type_t buttonType = static_cast<cef_mouse_button_type_t>(
      NWebInputDelegate::CefConverter("mousebutton", button));
  mouseEvent.modifiers = input_delegate_.GetModifiers(buttonType);

  if (browser_ && browser_->GetHost()) {
    if (NWebInputDelegate::IsMouseDown(action)) {
      browser_->GetHost()->SendMouseClickEvent(mouseEvent, buttonType, false, count);
    } else if (NWebInputDelegate::IsMouseUp(action)) {
      browser_->GetHost()->SendMouseClickEvent(mouseEvent, buttonType, true, 1);
    } else if (NWebInputDelegate::IsMouseMove(action)) {
      browser_->GetHost()->SendMouseMoveEvent(mouseEvent, false);
    }
  }
}
}  // namespace OHOS::NWeb

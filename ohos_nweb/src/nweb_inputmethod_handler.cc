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

#include "nweb_inputmethod_handler.h"

#include "input_method_utils.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "cef/include/cef_task.h"
#include "content/public/browser/browser_thread.h"
#include "libcef/browser/thread_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace OHOS::NWeb {
using namespace OHOS::MiscServices;

static constexpr char16_t DEL_CHAR = 127;

class OnTextChangedListenerImpl : public OnTextChangedListener {
 public:
  OnTextChangedListenerImpl(NWebInputMethodHandler* handler)
      : handler_(handler) {}

  // All following listenser callbacks should invoke call on UI thread
  void InsertText(const std::u16string& text) override {
    if (text.size() == 1 && text.front() == '\n') {
      return;
    }
    handler_->InsertText(text);
  }

  void DeleteForward(int32_t length) override {
    handler_->DeleteForward(length);
  }

  void DeleteBackward(int32_t length) override {
    handler_->DeleteBackward(length);
  }

  void SendKeyEventFromInputMethod(const KeyEvent& event) override {
    LOG(INFO) << "NWebInputMethodHandler::SendKeyEventFromInputMethod";
  }

  void SendKeyboardInfo(const KeyboardInfo& info) override {
    auto status = info.GetKeyboardStatus();
    if (status == KeyboardStatus::SHOW) {
      handler_->SetIMEStatus(true);
    } else if (status == KeyboardStatus::HIDE) {
      handler_->SetIMEStatus(false);
    } else if (status == KeyboardStatus::NONE) {
      handler_->SendEnterKeyEvent();
    }
  }

  void SetKeyboardStatus(bool status) override {
    handler_->SetIMEStatus(status);
  }

  void MoveCursor(const Direction direction) override {
    if (direction == Direction::NONE) {
      LOG(ERROR) << "NWebInputMethodHandler::MoveCursor got none direction";
      return;
    }
    handler_->MoveCursor(direction);
  }

  void HandleSetSelection(int32_t start, int32_t end) override {};
  void HandleExtendAction(int32_t action) override {};
  void HandleSelect(int32_t keyCode, int32_t cursorMoveSkip) override {};

 private:
  NWebInputMethodHandler* handler_;
};

class InputMethodTask : public CefTask {
 public:
  explicit InputMethodTask(base::OnceClosure closure) : closure_(std::move(closure)) {}

  InputMethodTask(const InputMethodTask&) = delete;
  InputMethodTask& operator=(const InputMethodTask&) = delete;

  virtual void Execute() override {
    std::move(closure_).Run();
    //closure_.Reset();
  }

 private:
  base::OnceClosure closure_;

  IMPLEMENT_REFCOUNTING(InputMethodTask);
};

NWebInputMethodHandler::NWebInputMethodHandler()
    : selected_from_(0), selected_to_(0) {}

NWebInputMethodHandler::~NWebInputMethodHandler() {}

void NWebInputMethodHandler::Attach(CefRefPtr<CefBrowser> browser,
                                    bool show_keyboard) {
  composing_text_.clear();
  browser_ = browser;
  if (inputmethod_listener_ == nullptr) {
    inputmethod_listener_ = new OnTextChangedListenerImpl(this);
  }

  if (show_keyboard && isAttached_) {
    LOG(INFO) << "Attach ShowCurrentInput" ;
    InputMethodController::GetInstance()->ShowCurrentInput();
  } else {
    InputMethodController::GetInstance()->Attach(inputmethod_listener_, show_keyboard);
    isAttached_ = true;
  }
}

void NWebInputMethodHandler::ShowTextInput() {
  LOG(INFO) << "NWebInputMethodHandler::ShowTextInput";
}

void NWebInputMethodHandler::HideTextInput() {
  if (!isAttached_) {
    LOG(INFO) << "keyboard is not attach";
    return;
  }
  InputMethodController::GetInstance()->HideTextInput();
  InputMethodController::GetInstance()->Close();
  isAttached_ = false;
}

void NWebInputMethodHandler::OnTextSelectionChanged(
    CefRefPtr<CefBrowser> browser,
    const CefString& selected_text,
    const CefRange& selected_range) {
  if (ime_text_composing_ && !composing_text_.empty()) {
    if (browser_ != nullptr && browser_->GetHost() != nullptr) {
      browser_->GetHost()->ImeFinishComposingText(false);
    }
  }
  selected_text_ = selected_text.ToString16();
  selected_from_ = selected_range.from;
  selected_to_ = selected_range.to;
  ime_text_composing_ = false;
  composing_text_.clear();
}

void NWebInputMethodHandler::SetIMEStatus(bool status) {
  if (browser_ != nullptr && browser_->GetHost() != nullptr) {
    CefRefPtr<CefTask> insert_task = new InputMethodTask(base::BindOnce(
        &NWebInputMethodHandler::SetIMEStatusOnUI, this, std::move(status)));
    browser_->GetHost()->PostTaskToUIThread(insert_task);
  }
}

void NWebInputMethodHandler::InsertText(const std::u16string& text) {
  if (text.empty()) {
    LOG(ERROR) << "insert text empty!";
    return;
  }

  if (browser_ != nullptr && browser_->GetHost() != nullptr) {
    CefRefPtr<CefTask> insert_task = new InputMethodTask(base::BindOnce(
        &NWebInputMethodHandler::InsertTextHandlerOnUI, this, std::move(text)));
    browser_->GetHost()->PostTaskToUIThread(insert_task);
  }
}

void NWebInputMethodHandler::DeleteBackward(int32_t length) {
  if (browser_ != nullptr && browser_->GetHost() != nullptr) {
    CefRefPtr<CefTask> delete_task = new InputMethodTask(base::BindOnce(
        &NWebInputMethodHandler::DeleteBackwardHandlerOnUI, this, length));
    browser_->GetHost()->PostTaskToUIThread(delete_task);
  }
}

void NWebInputMethodHandler::DeleteForward(int32_t length) {
  if (browser_ != nullptr && browser_->GetHost() != nullptr) {
    CefRefPtr<CefTask> delete_task = new InputMethodTask(base::BindOnce(
        &NWebInputMethodHandler::DeleteForwardHandlerOnUI, this, length));
    browser_->GetHost()->PostTaskToUIThread(delete_task);
  }
}

void NWebInputMethodHandler::SetIMEStatusOnUI(bool status) {
  if (!status && ime_text_composing_) {
    browser_->GetHost()->ImeFinishComposingText(false);
    ime_text_composing_ = false;
    composing_text_.clear();
  }
  ime_shown_ = status;
}

void NWebInputMethodHandler::InsertTextHandlerOnUI(const std::u16string& text) {
  if (text.empty()) {
    LOG(ERROR) << "insert text empty!";
    return;
  }

  CefKeyEvent keyEvent;
  keyEvent.windows_key_code = ui::VKEY_PROCESSKEY;
  keyEvent.modifiers = 0;
  keyEvent.is_system_key = false;
  keyEvent.type = KEYEVENT_RAWKEYDOWN;
  browser_->GetHost()->SendKeyEvent(keyEvent);

  if (!ime_text_composing_) {
    ime_text_composing_ = true;
    composing_text_.clear();
  }
  composing_text_.append(text);
  browser_->GetHost()->ImeCommitText(composing_text_,
    CefRange(UINT32_MAX, UINT32_MAX), 0);

  // no selection
  ime_text_composing_ = false;
  selected_from_ += static_cast<int>(composing_text_.length());
  selected_to_ += static_cast<int>(composing_text_.length());
  composing_text_.clear();

  keyEvent.type = KEYEVENT_KEYUP;
  browser_->GetHost()->SendKeyEvent(keyEvent);
}

void NWebInputMethodHandler::DeleteBackwardHandlerOnUI(int32_t length) {
  CefKeyEvent keyEvent;
  keyEvent.windows_key_code = ui::VKEY_DELETE;
  keyEvent.native_key_code = static_cast<int>(ScanKeyCode::DELETE_SCAN_CODE);
  keyEvent.modifiers = 0;
  keyEvent.is_system_key = false;
  keyEvent.character = keyEvent.unmodified_character = DEL_CHAR;

  if (!browser_ || !browser_->GetHost()) {
    LOG(ERROR) << "delete backward browser get failed";
    return;
  }

  for (int32_t i = 0; i < length; i++) {
    keyEvent.type = KEYEVENT_RAWKEYDOWN;
    browser_->GetHost()->SendKeyEvent(keyEvent);
    keyEvent.type = KEYEVENT_CHAR;
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }
  keyEvent.type = KEYEVENT_KEYUP;
  browser_->GetHost()->SendKeyEvent(keyEvent);
}

void NWebInputMethodHandler::DeleteForwardHandlerOnUI(int32_t length) {
  CefKeyEvent keyEvent;
  keyEvent.windows_key_code = ui::VKEY_BACK;
  keyEvent.native_key_code = static_cast<int>(ScanKeyCode::BACKSPACE_SCAN_CODE);
  keyEvent.modifiers = 0;
  keyEvent.is_system_key = false;
  keyEvent.character = keyEvent.unmodified_character = DEL_CHAR;

  if (!browser_ || !browser_->GetHost()) {
    LOG(ERROR) << "delete forward browser get failed";
    return;
  }

  for (int32_t i = 0; i < length; i++) {
    keyEvent.type = KEYEVENT_RAWKEYDOWN;
    browser_->GetHost()->SendKeyEvent(keyEvent);
    keyEvent.type = KEYEVENT_CHAR;
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }
  keyEvent.type = KEYEVENT_KEYUP;
  browser_->GetHost()->SendKeyEvent(keyEvent);
}

void NWebInputMethodHandler::SendEnterKeyEvent() {
  CefKeyEvent keyEvent;
  keyEvent.windows_key_code = ui::VKEY_RETURN;
  keyEvent.native_key_code = static_cast<int>(ScanKeyCode::ENTER_SCAN_CODE);

  keyEvent.type = KEYEVENT_KEYDOWN;
  keyEvent.character = '\r';
  keyEvent.modifiers = 0;
  keyEvent.is_system_key = false;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }

  keyEvent.type = KEYEVENT_CHAR;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }

  keyEvent.type = KEYEVENT_KEYUP;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }
}

void NWebInputMethodHandler::MoveCursor(const Direction direction) {
  LOG(INFO) << "NWebInputMethodHandler::MoveCursor called";
  CefKeyEvent keyEvent;

  switch (direction) {
    case Direction::UP: {
      keyEvent.windows_key_code = ui::VKEY_UP;
      keyEvent.native_key_code = static_cast<int>(ScanKeyCode::UP_SCAN_CODE);
      break;
    }
    case Direction::LEFT: {
      keyEvent.windows_key_code = ui::VKEY_LEFT;
      keyEvent.native_key_code = static_cast<int>(ScanKeyCode::LEFT_SCAN_CODE);
      break;
    }
    case Direction::RIGHT: {
      keyEvent.windows_key_code = ui::VKEY_RIGHT;
      keyEvent.native_key_code = static_cast<int>(ScanKeyCode::RIGHT_SCAN_CODE);
      break;
    }
    case Direction::DOWN: {
      keyEvent.windows_key_code = ui::VKEY_DOWN;
      keyEvent.native_key_code = static_cast<int>(ScanKeyCode::DOWN_SCAN_CODE);
      break;
    }
    default: {
      LOG(ERROR) << "invalid direction";
      return;
    }
  }

  keyEvent.type = KEYEVENT_KEYDOWN;
  keyEvent.modifiers = 0;
  keyEvent.is_system_key = false;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }

  keyEvent.type = KEYEVENT_KEYUP;
  if (browser_ && browser_->GetHost()) {
    browser_->GetHost()->SendKeyEvent(keyEvent);
  }
}
}  // namespace OHOS::NWeb

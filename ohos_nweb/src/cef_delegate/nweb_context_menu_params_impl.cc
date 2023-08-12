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
#include <unordered_map>
#include "nweb_context_menu_params_impl.h"

#include "nweb_context_menu_params.h"

using namespace OHOS::NWeb;

namespace {
using CmTf = NWebContextMenuParams::ContextMenuTypeFlags;
using CmMt = NWebContextMenuParams::ContextMenuMediaType;
using CmEf = NWebContextMenuParams::ContextMenuEditStateFlags;
using QmEf = NWebQuickMenuParams::QuickMenuEditStateFlags;
const std::unordered_map<int, int> kCmTypeFlagMap = {
  {CM_TYPEFLAG_NONE, CmTf::CM_TF_NONE},
  {CM_TYPEFLAG_PAGE, CmTf::CM_TF_PAGE},
  {CM_TYPEFLAG_FRAME, CmTf::CM_TF_FRAME},
  {CM_TYPEFLAG_LINK, CmTf::CM_TF_LINK},
  {CM_TYPEFLAG_MEDIA, CmTf::CM_TF_MEDIA},
  {CM_TYPEFLAG_SELECTION, CmTf::CM_TF_SELECTION},
  {CM_TYPEFLAG_EDITABLE, CmTf::CM_TF_EDITABLE},
};

const std::unordered_map<int, int> kCmMediaTypeMap = {
  {CM_MEDIATYPE_NONE, CmMt::CM_MT_NONE},
  {CM_MEDIATYPE_IMAGE, CmMt::CM_MT_IMAGE},
  {CM_MEDIATYPE_VIDEO, CmMt::CM_MT_VIDEO},
  {CM_MEDIATYPE_AUDIO, CmMt::CM_MT_AUDIO},
  {CM_MEDIATYPE_FILE, CmMt::CM_MT_FILE},
  {CM_MEDIATYPE_PLUGIN, CmMt::CM_MT_PLUGIN},
};

const std::unordered_map<int, int> kCmEditStateFlagsMap = {
  {CM_EDITFLAG_NONE, CmEf::CM_ES_NONE},
  {CM_EDITFLAG_CAN_UNDO, CmEf::CM_ES_CAN_UNDO},
  {CM_EDITFLAG_CAN_REDO, CmEf::CM_ES_CAN_REDO},
  {CM_EDITFLAG_CAN_CUT, CmEf::CM_ES_CAN_CUT},
  {CM_EDITFLAG_CAN_COPY, CmEf::CM_ES_CAN_COPY},
  {CM_EDITFLAG_CAN_PASTE, CmEf::CM_ES_CAN_PASTE},
  {CM_EDITFLAG_CAN_DELETE, CmEf::CM_ES_CAN_DELETE},
  {CM_EDITFLAG_CAN_SELECT_ALL, CmEf::CM_ES_CAN_SELECT_ALL},
  {CM_EDITFLAG_CAN_TRANSLATE, CmEf::CM_ES_CAN_TRANSLATE},
};

const std::unordered_map<int, int> kQmEditStateFlagsMap = {
  {QM_EDITFLAG_NONE, QmEf::QM_EF_NONE},
  {QM_EDITFLAG_CAN_ELLIPSIS, QmEf::QM_EF_CAN_ELLIPSIS},
  {QM_EDITFLAG_CAN_CUT, QmEf::QM_EF_CAN_CUT},
  {QM_EDITFLAG_CAN_COPY, QmEf::QM_EF_CAN_COPY},
  {QM_EDITFLAG_CAN_PASTE, QmEf::QM_EF_CAN_PASTE},
  {QM_EDITFLAG_CAN_SELECT_ALL, QmEf::QM_EF_CAN_SELECT_ALL},
};

const std::unordered_map<int, int> KMenuEventFlagsMap = {
  {EF_NONE, EVENTFLAG_NONE},
  {EF_CAPS_LOCK_ON, EVENTFLAG_CAPS_LOCK_ON},
  {EF_SHIFT_DOWN, EVENTFLAG_SHIFT_DOWN},
  {EF_CONTROL_DOWN, EVENTFLAG_CONTROL_DOWN},
  {EF_ALT_DOWN, EVENTFLAG_ALT_DOWN},
  {EF_LEFT_MOUSE_BUTTON, EVENTFLAG_LEFT_MOUSE_BUTTON},
  {EF_MIDDLE_MOUSE_BUTTON, EVENTFLAG_MIDDLE_MOUSE_BUTTON},
  {EF_RIGHT_MOUSE_BUTTON, EVENTFLAG_RIGHT_MOUSE_BUTTON},
};

const std::unordered_map<int32_t, cef_menu_id_t> KMenuCommandIdMap = {
  {CI_IMAGE_COPY, MENU_ID_IMAGE_COPY},
};

cef_menu_id_t ConvertCommandId(int32_t id) {
  std::unordered_map<int32_t, cef_menu_id_t>::const_iterator iter =
    KMenuCommandIdMap.find(id);
  if (iter != KMenuCommandIdMap.end()) {
    return iter->second;
  }
  return MENU_ID_CUSTOM_FIRST;
}

int32_t ConvertMenuFlags(int32_t value,
  const std::unordered_map<int, int>& flags_map) {
  int32_t result = 0;
  for (auto& kv : flags_map) {
    if ((kv.first & value) != 0) {
      result |= kv.second;
    }
  }
  return result;
}

CmMt ConvertContextMenuMediaType(
  CefContextMenuParams::MediaType value) {
  std::unordered_map<int, int>::const_iterator iter = 
    kCmMediaTypeMap.find(static_cast<int32_t>(value));
  if (iter != kCmMediaTypeMap.end()) {
    return static_cast<CmMt>(iter->second);
  }
  return CmMt::CM_MT_NONE;
}
}

namespace OHOS::NWeb {
NWebContextMenuParamsImpl::NWebContextMenuParamsImpl(
  CefRefPtr<CefContextMenuParams> params) : params_(params) {}

int32_t NWebContextMenuParamsImpl::GetXCoord() {
  if (params_ != nullptr) {
    return params_->GetXCoord();
  }
  return -1;
}

int32_t NWebContextMenuParamsImpl::GetYCoord() {
  if (params_ != nullptr) {
    return params_->GetYCoord();
  }
  return -1;
}

int32_t NWebContextMenuParamsImpl::GetContextMenuTypeFlags() {
  if (params_ != nullptr) {
    return ConvertMenuFlags(params_->GetTypeFlags(), kCmTypeFlagMap);
  }
  return 0;
}

std::string NWebContextMenuParamsImpl::GetLinkUrl() {
  if (params_ != nullptr) {
    return params_->GetLinkUrl().ToString();
  }
  return std::string();
}
 
std::string NWebContextMenuParamsImpl::GetUnfilteredLinkUrl() {
  if (params_ != nullptr) {
    return params_->GetUnfilteredLinkUrl().ToString();
  }
  return std::string();
}
 
std::string NWebContextMenuParamsImpl::GetSourceUrl() {
  if (params_ != nullptr) {
    return params_->GetSourceUrl().ToString();
  }
  return std::string();
}

bool NWebContextMenuParamsImpl::HasImageContents() {
  if (params_ != nullptr) {
    return params_->HasImageContents();
  }
  return false;
}

std::string NWebContextMenuParamsImpl::GetTitleText() {
  if (params_ != nullptr) {
    return params_->GetTitleText();
  }
  return std::string();
}

std::string NWebContextMenuParamsImpl::GetPageUrl() {
  if (params_ != nullptr) {
    return params_->GetPageUrl();
  }
  return std::string();
}

CmMt NWebContextMenuParamsImpl::GetMediaType() {
  if (params_ != nullptr) {
    return ConvertContextMenuMediaType(params_->GetMediaType());
  }
  return CmMt::CM_MT_NONE;
}

bool NWebContextMenuParamsImpl::IsEditable() {
  if (params_ != nullptr) {
    return params_->HasImageContents();
  }
  return params_->IsEditable();
}

int32_t NWebContextMenuParamsImpl::GetEditStateFlags() {
  if (params_ != nullptr) {
    return ConvertMenuFlags(params_->GetEditStateFlags(), kCmEditStateFlagsMap);
  }
  return 0;
}

NWebQuickMenuParamsImpl::NWebQuickMenuParamsImpl(
  int32_t x, int32_t y, int32_t width, int32_t height, int32_t flags)
  : x_(x), y_(y), width_(width), height_(height),
    edit_flags_(ConvertMenuFlags(flags, kQmEditStateFlagsMap)) {}

int32_t NWebQuickMenuParamsImpl::GetXCoord() {
  return x_;
}

int32_t NWebQuickMenuParamsImpl::GetYCoord() {
  return y_;
}

int32_t NWebQuickMenuParamsImpl::GetWidth() {
  return width_;
}

int32_t NWebQuickMenuParamsImpl::GetHeight() {
  return height_;
}

int32_t NWebQuickMenuParamsImpl::GetEditStateFlags() {
  return edit_flags_;
}

std::shared_ptr<NWebTouchHandleState>
NWebQuickMenuParamsImpl::GetTouchHandleState(
  NWebTouchHandleState::TouchHandleType type) {
  switch (type) {
    case NWebTouchHandleState::TouchHandleType::INSERT_HANDLE:
      return insert_touch_handle_state_;
    case NWebTouchHandleState::TouchHandleType::SELECTION_BEGIN_HANDLE:
      return begin_touch_handle_state_;
    case NWebTouchHandleState::TouchHandleType::SELECTION_END_HANDLE:
      return end_touch_handle_state_;
    default:
      return nullptr;
  }
}

void NWebQuickMenuParamsImpl::SetTouchHandleState(
    std::shared_ptr<NWebTouchHandleState> state,
    NWebTouchHandleState::TouchHandleType type) {
  switch (type) {
    case NWebTouchHandleState::TouchHandleType::INSERT_HANDLE:
      insert_touch_handle_state_ = state;
      break;
    case NWebTouchHandleState::TouchHandleType::SELECTION_BEGIN_HANDLE:
      begin_touch_handle_state_ = state;
      break;
    case NWebTouchHandleState::TouchHandleType::SELECTION_END_HANDLE:
      end_touch_handle_state_ = state;
      break;
    default:
      return;
  }
}

NWebContextMenuCallbackImpl::NWebContextMenuCallbackImpl(
  CefRefPtr<CefRunContextMenuCallback> callback) : callback_(callback) {}

void NWebContextMenuCallbackImpl::Continue(
  int32_t commandId, MenuEventFlags flag) {
  if (callback_ != nullptr) {
    int32_t event_flag = 
      ConvertMenuFlags(static_cast<int32_t>(flag), KMenuEventFlagsMap);
    callback_->Continue(ConvertCommandId(commandId),
                        static_cast<cef_event_flags_t>(event_flag));
  }
}

void NWebContextMenuCallbackImpl::Cancel() {
  if (callback_ != nullptr) {
    callback_->Cancel();
  }
}

NWebQuickMenuCallbackImpl::NWebQuickMenuCallbackImpl(
  CefRefPtr<CefRunQuickMenuCallback> callback) : callback_(callback) {}

void NWebQuickMenuCallbackImpl::Continue(
  int32_t commandId, MenuEventFlags flag) {
  if (callback_ != nullptr) {
    int32_t event_flag = 
      ConvertMenuFlags(static_cast<int32_t>(flag), KMenuEventFlagsMap);
    callback_->Continue(commandId,
                        static_cast<cef_event_flags_t>(event_flag));
  }
}

void NWebQuickMenuCallbackImpl::Cancel() {
  if (callback_ != nullptr) {
    callback_->Cancel();
  }
}
}
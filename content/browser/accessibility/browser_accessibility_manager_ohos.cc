/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "content/browser/accessibility/browser_accessibility_manager_ohos.h"
#include "content/browser/accessibility/browser_accessibility_ohos.h"

namespace content {
const int32_t kInvalidAccessibilityId = -1;
std::function<int32_t()> g_accessibility_id_generator;

BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerOHOS(initial_tree, delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerOHOS(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void BrowserAccessibilityManagerOHOS::HandleFocusChanged(
    int32_t accessibilityId) {
  if (accessibilityId == kInvalidAccessibilityId)
    return;
  SendAccessibilityEvent(accessibilityId,
                         OHOS::NWeb::AccessibilityEventType::FOCUS);
  MoveAccessibilityFocusToId(accessibilityId);
}

void BrowserAccessibilityManagerOHOS::RegisterAccessibilityIdGenerator(
    std::function<int32_t()> accessibilityIdGenerator) {
  if (g_accessibility_id_generator == nullptr) {
    g_accessibility_id_generator = accessibilityIdGenerator;
  }
}

int32_t BrowserAccessibilityManagerOHOS::GenerateAccessibilityId() {
  if (g_accessibility_id_generator != nullptr) {
    return g_accessibility_id_generator();
  }
  return kInvalidAccessibilityId;
}

void BrowserAccessibilityManagerOHOS::FireFocusEvent(
    BrowserAccessibility* node) {
  BrowserAccessibilityManager::FireFocusEvent(node);
  BrowserAccessibilityOHOS* nodeOHOS =
      static_cast<BrowserAccessibilityOHOS*>(node);
  if (!nodeOHOS) {
    return;
  }
  HandleFocusChanged(nodeOHOS->GetAccessibilityId());
}

void BrowserAccessibilityManagerOHOS::FireBlinkEvent(
    ax::mojom::Event event_type,
    BrowserAccessibility* node,
    int action_request_id) {
  BrowserAccessibilityOHOS* nodeOHOS =
      static_cast<BrowserAccessibilityOHOS*>(node);
  if (!nodeOHOS) {
    return;
  }
  switch (event_type) {
    case ax::mojom::Event::kClicked:
      SendAccessibilityEvent(nodeOHOS->GetAccessibilityId(),
                             OHOS::NWeb::AccessibilityEventType::CLICK);
      break;
    case ax::mojom::Event::kHover:
      HandleHover(nodeOHOS->GetAccessibilityId());
      return;
    case ax::mojom::Event::kTextChanged:
      SendAccessibilityEvent(nodeOHOS->GetAccessibilityId(),
                             OHOS::NWeb::AccessibilityEventType::TEXT_CHANGE);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerOHOS::RegisterAccessibilityEventListener(
    std::shared_ptr<OHOS::NWeb::NWebAccessibilityEventCallback>
        accessibilityEventListener) {
  accessibilityEventListener_ = accessibilityEventListener;
}

bool BrowserAccessibilityManagerOHOS::MoveAccessibilityFocusToId(
    int32_t newAccessibilityFocusId) {
  if (accessibilityFocusId_ == newAccessibilityFocusId) {
    return false;
  }
  MoveAccessibilityFocus(accessibilityFocusId_, newAccessibilityFocusId);
  accessibilityFocusId_ = newAccessibilityFocusId;
  SendAccessibilityEvent(
      accessibilityFocusId_,
      OHOS::NWeb::AccessibilityEventType::ACCESSIBILITY_FOCUSED);
  return true;
}

void BrowserAccessibilityManagerOHOS::MoveAccessibilityFocus(
    int32_t oldId,
    int32_t newId) const {
  auto oldNode = BrowserAccessibilityOHOS::GetFromAccessibilityId(oldId);
  if (oldNode && oldNode->manager())
    oldNode->manager()->ClearAccessibilityFocus(*oldNode);

  auto node = BrowserAccessibilityOHOS::GetFromAccessibilityId(newId);
  if (!node)
    return;
  node->manager()->SetAccessibilityFocus(*node);
}

void BrowserAccessibilityManagerOHOS::SendAccessibilityEvent(
    int32_t accessibilityId,
    OHOS::NWeb::AccessibilityEventType eventType) {
  if (accessibilityEventListener_ != nullptr &&
      eventType != OHOS::NWeb::AccessibilityEventType::UNKNOWN) {
    accessibilityEventListener_->OnAccessibilityEvent(
        accessibilityId, static_cast<int32_t>(eventType));
  }

  if (eventType == OHOS::NWeb::AccessibilityEventType::HOVER_ENTER_EVENT) {
    BrowserAccessibility* lastHoverNode = GetFromID(lastHoverId_);
    if (lastHoverNode) {
      SendAccessibilityEvent(
          lastHoverId_, OHOS::NWeb::AccessibilityEventType::HOVER_EXIT_EVENT);
    }
    lastHoverId_ = accessibilityId;
  }
}

void BrowserAccessibilityManagerOHOS::HandleHover(int32_t id) {
  if (lastHoverId_ == id) {
    LOG(INFO) << "The same node is hovered, id " << id;
    return;
  }

  SendAccessibilityEvent(id,
                         OHOS::NWeb::AccessibilityEventType::HOVER_ENTER_EVENT);
  MoveAccessibilityFocusToIdAndRefocusIfNeeded(id);
}

void BrowserAccessibilityManagerOHOS::
    MoveAccessibilityFocusToIdAndRefocusIfNeeded(
        int32_t newAccessibilityFocusId) {
  if (newAccessibilityFocusId == accessibilityFocusId_) {
    SendAccessibilityEvent(
        newAccessibilityFocusId,
        OHOS::NWeb::AccessibilityEventType::ACCESSIBILITY_FOCUS_CLEARED);
    accessibilityFocusId_ = kInvalidAccessibilityId;
  }
  MoveAccessibilityFocusToId(newAccessibilityFocusId);
}
}  // namespace content

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

#include "nweb_web_storage_delegate.h"
#include <memory>
#include <thread>
#include <vector>
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "cef/include/cef_waitable_event.h"
#include "cef/libcef/common/time_util.h"
#include "url/gurl.h"

using namespace OHOS::NWeb;
using base::WaitableEvent;

namespace {
class WebStorageCompletionCallback : public CefCompletionCallback {
 public:
  WebStorageCompletionCallback(
      std::shared_ptr<WaitableEvent> event,
      std::shared_ptr<NWebValueCallback<bool>> callback)
      : event_(event), callback_(callback) {}
  void OnComplete() override {
    if (event_ != nullptr) {
      event_->Signal();
    }
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(true);
    }
  }

 private:
  std::shared_ptr<WaitableEvent> event_;
  std::shared_ptr<NWebValueCallback<bool>> callback_;
  IMPLEMENT_REFCOUNTING(WebStorageCompletionCallback);
};

class GetOriginsCallback : public CefGetOriginsCallback {
 public:
  GetOriginsCallback(std::shared_ptr<WaitableEvent> event,
    std::shared_ptr<NWebGetOriginsCallback> callback)
    : event_(event), callback_(callback) {}
  void OnComplete() override {
    NWebWebStorageOrigin items;
    for (size_t i = 0; i < origins_.size(); i++) {
      items.SetOrigin(origins_[i]);
      items.SetQuota(quotas_[i]);
      items.SetUsage(usages_[i]);
      results_.push_back(items);
    }
    if (event_ != nullptr) {
      event_->Signal();
    }
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(results_);
    }
  }

  void OnOrigins(std::vector<CefString>& origins) override {
    for (size_t i = 0; i < origins.size(); i++) {
      std::string origin = origins[i].ToString();
      origins_.push_back(origin);
    }
  }

  void OnUsages(std::vector<CefString>& usages) override {
    for (size_t i = 0; i < usages.size(); i++) {
      long usage = atol(usages[i].ToString().c_str());
      usages_.push_back(usage);
    }
  }

  void OnQuotas(std::vector<CefString>& quotas) override {
    for (size_t i = 0; i < quotas.size(); i++) {
      long quota = atol(quotas[i].ToString().c_str());
      quotas_.push_back(quota);
    }
  }

  std::vector<NWebWebStorageOrigin> GetWebStorageOrigin() const { return results_; }

 private:
  std::shared_ptr<WaitableEvent> event_;
  std::shared_ptr<NWebGetOriginsCallback> callback_;
  std::vector<std::string> origins_;
  std::vector<long> usages_;
  std::vector<long> quotas_;
  std::vector<NWebWebStorageOrigin> results_;

  IMPLEMENT_REFCOUNTING(GetOriginsCallback);
};

class GetOriginUsageOrQuotaCallback : public CefGetOriginUsageOrQuotaCallback {
 public:
  GetOriginUsageOrQuotaCallback(std::shared_ptr<WaitableEvent> event,
    std::shared_ptr<NWebValueCallback<long>> callback)
    : event_(event), callback_(callback), nums_(-1) {}
  void OnComplete(int64 nums) override {
    nums_ = nums;
    if (event_ != nullptr) {
      event_->Signal();
    }
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(nums_);
    }
  }

  long GetUsageOrQuota() const { return nums_; }

 private:
  std::shared_ptr<WaitableEvent> event_;
  std::shared_ptr<NWebValueCallback<long>> callback_;
  long nums_;

  IMPLEMENT_REFCOUNTING(GetOriginUsageOrQuotaCallback);
};
}

namespace OHOS::NWeb {
CefRefPtr<CefWebStorage>
NWebWebStorageDelegate::GetGlobalWebStorage() {
  if (!web_storage_) {
    web_storage_ = CefWebStorage::GetGlobalManager(nullptr);
  }
  return web_storage_;
}

void NWebWebStorageDelegate::DeleteAllData() {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return;
  }
  web_storage->DeleteAllData();
}

int NWebWebStorageDelegate::DeleteOrigin(const std::string& origin) {
  GURL gurl = GURL(origin);
  if (gurl.is_empty() || !gurl.is_valid()) {
    return OHOS::NWeb::NWEB_INVALID_ORIGIN;
  }
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return NWEB_ERR;
  }
  web_storage->DeleteOrigin(origin);
  return NWEB_OK;
}

void NWebWebStorageDelegate::GetOrigins(std::shared_ptr<NWebGetOriginsCallback> callback) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return;
  }
  web_storage->GetOrigins(new GetOriginsCallback(nullptr, callback));
}

void NWebWebStorageDelegate::GetOrigins(std::vector<NWebWebStorageOrigin>& origins) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return;
  }
  std::shared_ptr<base::WaitableEvent> completion =
      std::make_shared<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  CefRefPtr<GetOriginsCallback> callback = new GetOriginsCallback(completion, nullptr);
  web_storage->GetOrigins(callback);
  completion->Wait();
  origins = callback->GetWebStorageOrigin();
}

void NWebWebStorageDelegate::GetOriginQuota(const std::string& origin,
                            std::shared_ptr<NWebValueCallback<long>> callback) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return;
  }
  web_storage->GetOriginQuota(CefString(origin), new GetOriginUsageOrQuotaCallback(nullptr, callback));
}

long NWebWebStorageDelegate::GetOriginQuota(const std::string& origin) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return NWEB_ERR;
  }
  GURL gurl = GURL(origin);
  if (gurl.is_empty() || !gurl.is_valid()) {
    return NWEB_INVALID_ORIGIN;
  }
  std::shared_ptr<base::WaitableEvent> completion =
      std::make_shared<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  CefRefPtr<GetOriginUsageOrQuotaCallback> callback = new GetOriginUsageOrQuotaCallback(completion, nullptr);
  web_storage->GetOriginQuota(CefString(origin), callback);
  completion->Wait();
  return callback->GetUsageOrQuota();
}

void NWebWebStorageDelegate::GetOriginUsage(const std::string& origin,
                            std::shared_ptr<NWebValueCallback<long>> callback) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return;
  }
  web_storage->GetOriginUsage(CefString(origin), new GetOriginUsageOrQuotaCallback(nullptr, callback));
}

long NWebWebStorageDelegate::GetOriginUsage(const std::string& origin) {
  CefRefPtr<CefWebStorage> web_storage = GetGlobalWebStorage();
  if (web_storage == nullptr) {
    return NWEB_ERR;
  }
  GURL gurl = GURL(origin);
  if (gurl.is_empty() || !gurl.is_valid()) {
    return NWEB_INVALID_ORIGIN;
  }
  std::shared_ptr<base::WaitableEvent> completion =
      std::make_shared<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  CefRefPtr<GetOriginUsageOrQuotaCallback> callback = new GetOriginUsageOrQuotaCallback(completion, nullptr);
  web_storage->GetOriginUsage(CefString(origin), callback);
  completion->Wait();
  return callback->GetUsageOrQuota();
}
}  // namespace OHOS::NWeb

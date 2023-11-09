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

#ifndef NWEB_WEB_STORAGE_IMPL_H
#define NWEB_WEB_STORAGE_IMPL_H

#include "nweb_errors.h"
#include "nweb_web_storage.h"
#include "nweb_web_storage_delegate_interface.h"

namespace OHOS::NWeb {
class NWebWebStorageImpl : public NWebWebStorage {
 public:
  NWebWebStorageImpl();
  ~NWebWebStorageImpl() = default;

  void DeleteAllData() override;
  int DeleteOrigin(const std::string& origin) override;
  void GetOrigins(std::shared_ptr<NWebGetOriginsCallback> callback) override;
  std::vector<NWebWebStorageOrigin> GetOrigins() override;
  void GetOriginQuota(const std::string& origin,
      std::shared_ptr<NWebValueCallback<long>> callback) override;
  long GetOriginQuota(const std::string& origin) override;
  void GetOriginUsage(const std::string& origin,
      std::shared_ptr<NWebValueCallback<long>> callback) override;
  long GetOriginUsage(const std::string& origin) override;
 private:
  std::shared_ptr<NWebWebStorageDelegateInterface> delegate_;
};
}  // namespace OHOS::NWeb

#endif  // NWEB_WEB_STORAGE_IMPL_H

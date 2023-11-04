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

#ifndef BATTERY_MGR_CLIENT_ADAPTER_H
#define BATTERY_MGR_CLIENT_ADAPTER_H

#include <functional>

namespace OHOS::NWeb {
class WebBatteryInfo {
public:
    virtual ~WebBatteryInfo() = default;

    virtual double GetLevel() = 0;

    virtual bool IsCharging() = 0;

    virtual int DisChargingTime() = 0;

    virtual int ChargingTime() = 0;
};

using BatteryEventCallback = std::function<void(WebBatteryInfo&)>;

class BatteryMgrClientAdapter {
public:
    BatteryMgrClientAdapter() = default;

    virtual ~BatteryMgrClientAdapter() = default;
    
    virtual void RegBatteryEvent(const BatteryEventCallback&& eventCallback) = 0;
  
    virtual bool StartListen() = 0;

    virtual void StopListen() = 0;

    virtual std::unique_ptr<WebBatteryInfo> RequestBatteryInfo() = 0;
};
}  // namespace OHOS::NWeb

#endif  // BATTERY_MGR_CLIENT_ADAPTER_
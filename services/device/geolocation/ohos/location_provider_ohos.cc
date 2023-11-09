// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/ohos/location_provider_ohos.h"

#include <memory>

#include <common_utils.h>
#include <constant_definition.h>
#include <system_ability_definition.h>

#include "base/bind.h"
#include "base/memory/singleton.h"

namespace device {
class GeolocationManager;
// LocationProviderOhos
LocationProviderOhos::LocationProviderOhos() {
  locator_callback_ = new LocationProviderCallback();
}

LocationProviderOhos::~LocationProviderOhos() {
  StopProvider();
  if (locator_ != nullptr) {
    locator_.reset();
  }
  if (locator_callback_ != nullptr) {
    locator_callback_ = nullptr;
  }
}

void LocationProviderOhos::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  callback_ = callback;

  locator_callback_->SetUpdateCallback(base::BindRepeating(
      &LocationProviderOhos::ProviderUpdateCallback, base::Unretained(this)));
}

void LocationProviderOhos::ProviderUpdateCallback(
    const mojom::Geoposition& position) {
  if (!callback_.is_null() && is_running_)
    callback_.Run(this, position);
}

void LocationProviderOhos::StartProvider(bool high_accuracy) {
  StopProvider();
  RequestLocationUpdate(high_accuracy);
}

void LocationProviderOhos::StopProvider() {
  if (!is_running_)
    return;
  is_running_ = false;
  OHOS::sptr<OHOS::Location::ILocatorCallback> locator_call_back =
      locator_callback_;
  locator_->StopLocating(locator_call_back);
}

const mojom::Geoposition& LocationProviderOhos::GetPosition() {
  return locator_callback_->GetPosition();
}

void LocationProviderOhos::OnPermissionGranted() {
  // Nothing to do here.
}

void LocationProviderCallback::OnNewLocationAvailable(
    const std::unique_ptr<OHOS::Location::Location>& location) {
  mojom::Geoposition position;
  position.latitude = location->GetLatitude();
  position.longitude = location->GetLongitude();
  // location->GetTimeStamp() has no value now, so that temporarily passed
  // value 10.
  position.timestamp = base::Time::FromDoubleT(10);
  position.altitude = location->GetAltitude();
  position.accuracy = location->GetAccuracy();
  position.heading = location->GetDirection();
  position.speed = location->GetSpeed();

  NewGeopositionReport(position);
}

void LocationProviderCallback::OnNewErrorAvailable(std::string message) {
  mojom::Geoposition position_error;
  position_error.error_code =
      mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  position_error.error_message = message;

  NewGeopositionReport(position_error);
}

void LocationProviderOhos::RequestLocationUpdate(bool high_accuracy) {
  LOG(INFO) << "LocationProviderOhos::RequestLocationUpdate";
  is_running_ = true;
  CreateLocationManagerIfNeeded();
  if (locator_ == nullptr) {
    LOG(ERROR) << "Locator is null. Can not get location";
    locator_callback_->OnErrorReport(
        LocationProviderCallback::LOCATION_GET_FAILED);
    return;
  }

  std::unique_ptr<OHOS::Location::RequestConfig> requestConfig =
      std::make_unique<OHOS::Location::RequestConfig>();
  SetRequestConfig(requestConfig, high_accuracy);
  if (locator_->GetSwitchState() != 1) {
    LOG(ERROR) << "geolocation setting is not turned on";
    locator_callback_->OnErrorReport(
        LocationProviderCallback::LOCATION_GET_FAILED);
    return;
  }
  OHOS::sptr<OHOS::Location::ILocatorCallback> locator_call_back =
      locator_callback_;
  int ret = locator_->StartLocating(requestConfig, locator_call_back, "location.ILocator",
                                    0, 0);
  if (ret != 0) {
    LOG(ERROR) << "StartLocating failed. Can not get location";
    locator_callback_->OnErrorReport(
        LocationProviderCallback::LOCATION_GET_FAILED);
  }
}

void LocationProviderOhos::CreateLocationManagerIfNeeded() {
  if (locator_ != nullptr) {
    return;
  }
  locator_ = std::make_unique<OHOS::Location::LocatorProxy>(
      OHOS::Location::CommonUtils::GetRemoteObject(
          OHOS::LOCATION_LOCATOR_SA_ID,
          OHOS::Location::CommonUtils::InitDeviceId()));
}

void LocationProviderOhos::SetRequestConfig(
    std::unique_ptr<OHOS::Location::RequestConfig>& requestConfig,
    bool high_accuracy) {
  requestConfig->SetPriority(OHOS::Location::PRIORITY_FAST_FIRST_FIX);
  requestConfig->SetScenario(OHOS::Location::SCENE_UNSET);
  requestConfig->SetTimeInterval(1);
  requestConfig->SetDistanceInterval(0);
  requestConfig->SetMaxAccuracy(50);
  requestConfig->SetFixNumber(0);
}

void LocationProviderCallback::NewGeopositionReport(
    const mojom::Geoposition& position) {
  last_position_ = position;
  if (!callback_.is_null())
    callback_.Run(position);
}

int LocationProviderCallback::OnRemoteRequest(uint32_t code,
                                              OHOS::MessageParcel& data,
                                              OHOS::MessageParcel& reply,
                                              OHOS::MessageOption& option) {
  if (data.ReadInterfaceToken() != GetDescriptor()) {
    LOG(INFO) << "invalid token.";
    return -1;
  }
  switch (code) {
    case RECEIVE_LOCATION_INFO_EVENT: {
      std::unique_ptr<OHOS::Location::Location> location =
          OHOS::Location::Location::Unmarshalling(data);
      OnLocationReport(location);
      break;
    }
    case RECEIVE_ERROR_INFO_EVENT: {
      break;
    }
    case RECEIVE_LOCATION_STATUS_EVENT: {
      OnLocatingStatusChange(0);
      break;
    }
    default: {
      break;
    }
  }
  return 0;
}

void LocationProviderCallback::OnLocationReport(
    const std::unique_ptr<OHOS::Location::Location>& location) {
  OnNewLocationAvailable(location);
}

void LocationProviderCallback::OnLocatingStatusChange(const int status) {}

void LocationProviderCallback::OnErrorReport(const int errorCode) {
  if (errorCode == LOCATION_GET_FAILED) {
    OnNewErrorAvailable("Failed to get location!");
  } else {
    OnNewErrorAvailable("Unknown error during the locating occured!");
  }
}

// static
std::unique_ptr<LocationProvider> NewSystemLocationProvider(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    GeolocationManager* geolocation_manager) {
  return base::WrapUnique(new LocationProviderOhos);
}

}  // namespace device

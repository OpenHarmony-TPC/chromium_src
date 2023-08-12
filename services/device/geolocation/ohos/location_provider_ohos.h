// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_OHOS_H_
#define SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_OHOS_H_

#include <memory>

#include <i_locator_callback.h>
#include <iremote_stub.h>
#include <location.h>
#include <locator_proxy.h>
#include <message_parcel.h>
#include <request_config.h>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {
class LocationProviderCallback
    : public OHOS::IRemoteStub<OHOS::Location::ILocatorCallback> {
 public:
  LocationProviderCallback() {}
  ~LocationProviderCallback() = default;

  enum LocationErrorCode {
    LOCATION_GET_SUCCESS = 0,
    LOCATION_GET_FAILED,
    LOCATION_UNKNOWN_ERROR,
  };

  typedef base::RepeatingCallback<void(const mojom::Geoposition&)>
      UpdateCallback;

  // ILocatorCallback implementation.
  virtual int OnRemoteRequest(uint32_t code,
                              OHOS::MessageParcel& data,
                              OHOS::MessageParcel& reply,
                              OHOS::MessageOption& option) override;
  void OnLocationReport(
      const std::unique_ptr<OHOS::Location::Location>& location) override;
  void OnLocatingStatusChange(const int status) override;
  void OnErrorReport(const int errorCode) override;

  void OnNewLocationAvailable(
      const std::unique_ptr<OHOS::Location::Location>& location);
  void OnNewErrorAvailable(std::string message);
  void SetUpdateCallback(const UpdateCallback& callback) {
    callback_ = callback;
  }
  const mojom::Geoposition& GetPosition() { return last_position_; }

 private:
  // Calls |callback_| with the new location.
  void NewGeopositionReport(const mojom::Geoposition& position);

  mojom::Geoposition last_position_;
  UpdateCallback callback_;
};

// Location provider for OpenHarmony using the platform provider over JNI.
class LocationProviderOhos : public LocationProvider {
 public:
  LocationProviderOhos();
  ~LocationProviderOhos() override;

  // LocationProvider implementation.
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const mojom::Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

  void ProviderUpdateCallback(const mojom::Geoposition& position);

 private:
  void RequestLocationUpdate(bool high_accuracy);
  void CreateLocationManagerIfNeeded();
  void SetRequestConfig(
      std::unique_ptr<OHOS::Location::RequestConfig>& requestConfig,
      bool high_accuracy);

  std::unique_ptr<OHOS::Location::LocatorProxy> locator_;

  LocationProviderUpdateCallback callback_;

  bool is_running_ = false;
  OHOS::sptr<LocationProviderCallback> locator_callback_ = nullptr;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_LOCATION_PROVIDER_OHOS_H_

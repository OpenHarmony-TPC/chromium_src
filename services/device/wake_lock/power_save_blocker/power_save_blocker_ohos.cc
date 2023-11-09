// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include "base/memory/ref_counted.h"
#include "base/logging.h"
#include "ohos_adapter_helper.h"

using namespace OHOS::NWeb;
namespace device {

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate();

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  std::unique_ptr<PowerMgrClientAdapter> power_mgr_client_ = nullptr;
  std::shared_ptr<RunningLockAdapter> lock_ = nullptr;
};

PowerSaveBlocker::Delegate::Delegate() {
  power_mgr_client_ =
    OHOS::NWeb::OhosAdapterHelper::GetInstance().CreatePowerMgrClientAdapter();
  if (power_mgr_client_ != nullptr) {
    lock_ = power_mgr_client_->CreateRunningLock(
      "nweb_lock", RunningLockAdapterType::SCREEN);
  }
}

void PowerSaveBlocker::Delegate::ApplyBlock() {
  if (lock_ != nullptr) {
    lock_->Lock(0);
  }
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  if (lock_ != nullptr) {
    lock_->UnLock();
  }
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate()),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  if (delegate_.get()) {
    delegate_->ApplyBlock();
  }
}

PowerSaveBlocker::~PowerSaveBlocker() {
  if (delegate_.get()) {
    delegate_->RemoveBlock();
  }
}

}  // namespace device

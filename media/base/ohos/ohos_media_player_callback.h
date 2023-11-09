// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_MEDIA_PLAYER_CALLBACK_H_
#define MEDIA_BASE_OHOS_MEDIA_PLAYER_CALLBACK_H_

#include <media_errors.h>
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/ohos/ohos_media_player_bridge.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class OHOSMediaPlayerBridge;

class OHOSMediaPlayerCallback : public OHOS::Media::PlayerCallback {
 public:
  OHOSMediaPlayerCallback(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<OHOSMediaPlayerBridge> media_player);

  OHOSMediaPlayerCallback(const OHOSMediaPlayerCallback&) = delete;
  OHOSMediaPlayerCallback& operator=(const OHOSMediaPlayerCallback&) = delete;

  virtual ~OHOSMediaPlayerCallback();

  void OnError(OHOS::Media::PlayerErrorType errorType,
               int32_t errorCode) override;
  void OnInfo(OHOS::Media::PlayerOnInfoType type,
              int32_t extra,
              const OHOS::Media::Format& infoBody = {}) override;
  bool IsUnsupportType(int32_t errorCode);
  bool IsFatalError(int32_t errorCode);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<OHOSMediaPlayerBridge> media_player_;
};

}  // namespace media

#endif  // MEDIA_BASE_OHOS_MEDIA_PLAYER_CALLBACK_H_

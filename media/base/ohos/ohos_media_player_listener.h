// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_MEDIA_PLAYER_LISTENER_H_
#define MEDIA_BASE_OHOS_MEDIA_PLAYER_LISTENER_H_

#include <surface.h>
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/ohos/ohos_media_player_bridge.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class OHOSMediaPlayerBridge;

class OHOSMediaPlayerListener : public OHOS::IBufferConsumerListener {
 public:
  OHOSMediaPlayerListener(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::WeakPtr<OHOSMediaPlayerBridge> media_player,
      OHOS::sptr<OHOS::Surface> impl);

  OHOSMediaPlayerListener(const OHOSMediaPlayerListener&) = delete;
  OHOSMediaPlayerListener& operator=(const OHOSMediaPlayerListener&) = delete;

  virtual ~OHOSMediaPlayerListener();
  void OnBufferAvailable() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<OHOSMediaPlayerBridge> media_player_;
  OHOS::wptr<OHOS::Surface> surface_;
  int64_t timestamp;
  OHOS::Rect damage;
};

}  // namespace media

#endif  // MEDIA_BASE_OHOS_MEDIA_PLAYER_LISTENER_H_

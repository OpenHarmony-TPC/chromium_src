// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ohos/ohos_media_player_listener.h"

#include <display_type.h>
namespace media {

OHOSMediaPlayerListener::OHOSMediaPlayerListener(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<OHOSMediaPlayerBridge> media_player,
    OHOS::sptr<OHOS::Surface> impl)
    : task_runner_(task_runner), media_player_(media_player), surface_(impl) {
  DCHECK(task_runner_.get());
  DCHECK(media_player_);
}

OHOSMediaPlayerListener::~OHOSMediaPlayerListener() {}

void OHOSMediaPlayerListener::OnBufferAvailable() {
  OHOS::sptr<OHOS::SurfaceBuffer> buffer;
  int32_t fence;
  OHOS::SurfaceError ret;
  auto surface_temp = surface_.promote();
  if (surface_temp == nullptr) {
    LOG(ERROR) << "surface is null";
    return;
  }

  ret = surface_temp->AcquireBuffer(buffer, fence, timestamp, damage);
  if (ret != OHOS::SURFACE_ERROR_OK) {
    LOG(ERROR) << "acquire buffer fail, ret=" << ret;
    return;
  }

  if (buffer->GetFormat() == PIXEL_FMT_RGBA_8888 ||
      buffer->GetFormat() == PIXEL_FMT_YCBCR_420_SP) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OHOSMediaPlayerBridge::OnBufferAvailable,
                                  media_player_, buffer));
  } else {
    LOG(ERROR) << "Unsupport format for:" << buffer->GetFormat();
    surface_temp->ReleaseBuffer(buffer, -1);
  }
}

}  // namespace media

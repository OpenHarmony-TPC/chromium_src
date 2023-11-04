// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ohos/ohos_media_player_callback.h"

namespace media {

OHOSMediaPlayerCallback::OHOSMediaPlayerCallback(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<OHOSMediaPlayerBridge> media_player)
    : task_runner_(task_runner), media_player_(media_player) {
  DCHECK(task_runner_.get());
  DCHECK(media_player_);
}

OHOSMediaPlayerCallback::~OHOSMediaPlayerCallback() {}

void OHOSMediaPlayerCallback::OnError(OHOS::Media::PlayerErrorType errorType,
                                      int32_t errorCode) {
  LOG(ERROR) << "media player error code=" << errorCode;
  int media_error_type =
      OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_INVALID_CODE;
  if (IsUnsupportType(errorCode)) {
    media_error_type =
        OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_FORMAT;
  } else if (IsFatalError(errorCode)) {
    media_error_type =
        OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_DECODE;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OHOSMediaPlayerBridge::OnError, media_player_,
                                media_error_type));
}

void OHOSMediaPlayerCallback::OnInfo(OHOS::Media::PlayerOnInfoType type,
                                     int32_t extra,
                                     const OHOS::Media::Format& infoBody) {
  (void)infoBody;
  switch (type) {
    case OHOS::Media::INFO_TYPE_EOS:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&OHOSMediaPlayerBridge::OnEnd, media_player_));
      break;
    case OHOS::Media::INFO_TYPE_STATE_CHANGE:
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&OHOSMediaPlayerBridge::OnPlayerStateUpdate,
                         media_player_,
                         static_cast<OHOS::Media::PlayerStates>(extra)));
      break;
    case OHOS::Media::INFO_TYPE_POSITION_UPDATE:
      break;
    case OHOS::Media::INFO_TYPE_MESSAGE:
      break;
    default:
      break;
  }
}

bool OHOSMediaPlayerCallback::IsUnsupportType(int32_t errorCode) {
  switch (errorCode) {
    case OHOS::Media::MSERR_UNSUPPORT:
    case OHOS::Media::MSERR_UNSUPPORT_AUD_SRC_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_AUD_CHANNEL_NUM:
    case OHOS::Media::MSERR_UNSUPPORT_AUD_ENC_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_AUD_PARAMS:
    case OHOS::Media::MSERR_UNSUPPORT_VID_SRC_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_VID_ENC_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_VID_PARAMS:
    case OHOS::Media::MSERR_UNSUPPORT_CONTAINER_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_PROTOCOL_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_VID_DEC_TYPE:
    case OHOS::Media::MSERR_UNSUPPORT_AUD_DEC_TYPE:
      return true;
  }
  return false;
}
bool OHOSMediaPlayerCallback::IsFatalError(int32_t errorCode) {
  switch (errorCode) {
    case OHOS::Media::MSERR_NO_MEMORY:
    case OHOS::Media::MSERR_SERVICE_DIED:
    case OHOS::Media::MSERR_CREATE_PLAYER_ENGINE_FAILED:
    case OHOS::Media::MSERR_CREATE_AVMETADATAHELPER_ENGINE_FAILED:
    case OHOS::Media::MSERR_AUD_DEC_FAILED:
    case OHOS::Media::MSERR_VID_DEC_FAILED:
    case OHOS::Media::MSERR_OPEN_FILE_FAILED:
    case OHOS::Media::MSERR_FILE_ACCESS_FAILED:
      return true;
  }
  return false;
}

}  // namespace media

// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ohos/ohos_media_player_bridge.h"

#include <display_type.h>
#include <sys/stat.h>
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace media {

static constexpr int PLAYER_INIT_OK = 0;
static constexpr int PLAYER_INIT_ERROR = -1;

OHOSMediaPlayerBridge::OHOSMediaPlayerBridge(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    const std::string& user_agent,
    bool hide_url_log,
    Client* client,
    bool allow_credentials,
    bool is_hls)
    : client_(client),
      url_(url),
      prepared_(false),
      pending_play_(false),
      should_seek_on_prepare_(false),
      should_set_volume_on_prepare_(false),
      seeking_on_playback_complete_(false) {}

OHOSMediaPlayerBridge::~OHOSMediaPlayerBridge() {
  if (player_) {
    player_->Release();
    player_ = nullptr;
  }

  cached_buffers_.clear();
}

int32_t OHOSMediaPlayerBridge::Initialize() {
  if (url_.SchemeIsBlob()) {
    NOTREACHED();
    return PLAYER_INIT_ERROR;
  }
  return PLAYER_INIT_OK;
}

void OHOSMediaPlayerBridge::Start() {
  if (!player_) {
    pending_play_ = true;
    Prepare();
  } else {
    if (prepared_) {
      StartInternal();
    } else {
      pending_play_ = true;
    }
  }
}

void OHOSMediaPlayerBridge::Prepare() {
  DCHECK(!player_);

  if (url_.SchemeIsBlob()) {
    NOTREACHED();
    return;
  }

  player_ = OHOS::Media::PlayerFactory::CreatePlayer();
  if (!player_) {
    LOG(ERROR) << "media create player failed";
    return;
  }

  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  if (!task_runner_) {
    LOG(ERROR) << "media task runner failed";
    return;
  }

  std::shared_ptr<OHOSMediaPlayerCallback> media_player_callback =
      std::make_shared<OHOSMediaPlayerCallback>(task_runner_,
                                                weak_factory_.GetWeakPtr());
  int32_t ret = player_->SetPlayerCallback(media_player_callback);
  if (ret != 0) {
    LOG(ERROR) << "SetPlayerCallback error::ret=" << ret;
    return;
  }

  if (url_.SchemeIsFile()) {
    ret = SetFdSource(url_.GetContent());
  } else {
    ret = player_->SetSource(url_.spec());
  }
  if (ret != 0) {
    LOG(ERROR) << "SetSource error::ret=" << ret;
    return;
  }

  consumer_surface_ = OHOS::Surface::CreateSurfaceAsConsumer();
  if (consumer_surface_ == nullptr) {
    LOG(ERROR) << "media create surface failed";
    return;
  }
  listener_ = new OHOSMediaPlayerListener(
      task_runner_, weak_factory_.GetWeakPtr(), consumer_surface_);
  consumer_surface_->RegisterConsumerListener(listener_);
  consumer_surface_->SetUserData(surfaceFormat,
                                 std::to_string(PIXEL_FMT_RGBA_8888));
  consumer_surface_->SetQueueSize(3);
  ret = player_->SetVideoSurface(consumer_surface_);
  if (ret != 0) {
    LOG(ERROR) << "SetVideoSurface error::ret=" << ret;
    consumer_surface_ = nullptr;
    return;
  }

  ret = player_->PrepareAsync();
  if (ret != 0) {
    LOG(ERROR) << "Prepare error::ret=" << ret << " url=" << url_.spec();
  }
}

void OHOSMediaPlayerBridge::StartInternal() {
  if (player_ && prepared_) {
    player_->Play();
  }
}

void OHOSMediaPlayerBridge::Pause() {
  if (player_ && player_state_ == OHOS::Media::PLAYER_STARTED) {
    int32_t ret = player_->Pause();
    if (ret != 0) {
      LOG(ERROR) << "Pause error::ret=" << ret;
    }
  }
}

void OHOSMediaPlayerBridge::SeekTo(base::TimeDelta time) {
  pending_seek_ = time;

  if (player_state_ == OHOS::Media::PLAYER_PLAYBACK_COMPLETE) {
    seeking_on_playback_complete_ = true;
    return;
  }

  if (!prepared_) {
    should_seek_on_prepare_ = true;
    return;
  }

  SeekInternal(time);
}

void OHOSMediaPlayerBridge::SeekInternal(base::TimeDelta time) {
  int32_t ret = player_->Seek(time.InMilliseconds(),
                              OHOS::Media::PlayerSeekMode::SEEK_CLOSEST);
  if (ret != 0) {
    LOG(ERROR) << "Seek error::ret=" << ret;
  }
}

void OHOSMediaPlayerBridge::SetVolume(float volume) {
  if (!prepared_) {
    should_set_volume_on_prepare_ = true;
    volume_ = volume;
    return;
  }

  if (player_) {
    int32_t ret = player_->SetVolume(volume, volume);
    if (ret != 0) {
      LOG(ERROR) << "SetVolume error::ret=" << ret;
    }
  }
}

void OHOSMediaPlayerBridge::PropagateDuration(base::TimeDelta duration) {
  duration_ = duration;
  client_->OnMediaDurationChanged(duration_);
}

base::TimeDelta OHOSMediaPlayerBridge::GetDuration() {
  if (player_ && prepared_) {
    int32_t time = -1;
    (void)player_->GetDuration(time);
    return base::Milliseconds(time);
  }
  return base::TimeDelta();
}

base::TimeDelta OHOSMediaPlayerBridge::GetMediaTime() {
  if (!player_ || !prepared_ || seeking_on_playback_complete_) {
    return pending_seek_;
  }

  int32_t time = -1;
  (void)player_->GetCurrentTime(time);
  return base::Milliseconds(time);
}

void OHOSMediaPlayerBridge::FinishPaint(int fd) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&OHOSMediaPlayerBridge::FinishPaint,
                                          weak_factory_.GetWeakPtr(), fd));
    return;
  }

#if defined(RK3568)
  if (cached_buffers_.front()->GetBufferHandle()->fd != fd) {
    LOG(ERROR) << "match fd error render fd=" << fd << "  browser fd="
               << cached_buffers_.front()->GetBufferHandle()->fd;
  }
#else
  if (cached_buffers_.front()->GetBufferHandle()->reserve[0] != fd) {
    LOG(ERROR) << "match fd error render fd=" << fd << "  browser fd="
               << cached_buffers_.front()->GetBufferHandle()->reserve[0];
  }
#endif
  OHOS::SurfaceError ret =
      consumer_surface_->ReleaseBuffer(cached_buffers_.front(), -1);
  if (ret != OHOS::SURFACE_ERROR_OK) {
    LOG(ERROR) << "release buffer fail, ret=" << ret;
  }
  cached_buffers_.pop_front();
}

void OHOSMediaPlayerBridge::SetPlaybackSpeed(
    OHOS::Media::PlaybackRateMode mode) {
  if (player_) {
    int32_t ret = player_->SetPlaybackSpeed(mode);
    if (ret != 0) {
      LOG(ERROR) << "SetPlaybackSpeed error::ret=" << ret;
    }
  }
}

void OHOSMediaPlayerBridge::OnEnd() {
  client_->OnPlaybackComplete();
}

void OHOSMediaPlayerBridge::OnError(int32_t error_code) {
  client_->OnError(error_code);
}

void OHOSMediaPlayerBridge::OnPlayerStateUpdate(
    OHOS::Media::PlayerStates player_state) {
  if (!player_) {
    return;
  }

  if (player_state_ == OHOS::Media::PLAYER_PLAYBACK_COMPLETE &&
      player_state != player_state_) {
    seeking_on_playback_complete_ = false;
  }

  player_state_ = player_state;
  if (player_state == OHOS::Media::PLAYER_PREPARED) {
    prepared_ = true;

    PropagateDuration(GetDuration());

    if (should_seek_on_prepare_) {
      SeekInternal(pending_seek_);
      pending_seek_ = base::Milliseconds(0);
      should_seek_on_prepare_ = false;
    }

    if (should_set_volume_on_prepare_) {
      SetVolume(volume_);
      should_set_volume_on_prepare_ = false;
    }

    if (pending_play_) {
      StartInternal();
      pending_play_ = false;
    }
  } else if (player_state == OHOS::Media::PLAYER_STATE_ERROR ||
             player_state == OHOS::Media::PLAYER_IDLE ||
             player_state == OHOS::Media::PLAYER_INITIALIZED ||
             player_state == OHOS::Media::PLAYER_PREPARING) {
    prepared_ = false;
  }
}

void OHOSMediaPlayerBridge::OnBufferAvailable(
    OHOS::sptr<OHOS::SurfaceBuffer> buffer) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OHOSMediaPlayerBridge::OnBufferAvailable,
                       weak_factory_.GetWeakPtr(), std::move(buffer)));
    return;
  }

#if defined(RK3568)
  int fd = buffer->GetBufferHandle()->fd;
#else
  int fd = buffer->GetBufferHandle()->reserve[0];
#endif
  if (fd <= 0) {
    LOG(ERROR) << "surface buffer fd error fd:" << fd;
    OHOS::SurfaceError ret = consumer_surface_->ReleaseBuffer(buffer, -1);
    if (ret != OHOS::SURFACE_ERROR_OK) {
      LOG(ERROR) << "release buffer fail, ret=" << ret;
    }
    return;
  }

#if defined(RK3568)
  client_->OnFrameAvailable(fd, buffer->GetSize(), buffer->GetWidth(),
                            buffer->GetHeight(), buffer->GetWidth(),
                            buffer->GetHeight(), buffer->GetFormat());
#else
  int32_t coded_height;
  int32_t coded_width;

  // video frame height must be 32*N
  const int step_height = 32;
  // argb format video frame should divided by 4
  const int argb_stride_step = 4;
  if (buffer->GetHeight() % step_height == 0) {
    coded_height = buffer->GetHeight();
  } else {
    coded_height = (buffer->GetHeight() / step_height + 1) * step_height;
  }
  if (buffer->GetFormat() == PIXEL_FMT_RGBA_8888) {
    coded_width = buffer->GetStride() / argb_stride_step;
  } else {
    coded_width = buffer->GetStride();
  }
  client_->OnFrameAvailable(fd, buffer->GetSize(), coded_width, coded_height,
                            buffer->GetWidth(), buffer->GetHeight(),
                            buffer->GetFormat());
#endif
  cached_buffers_.push_back(buffer);
}

int32_t OHOSMediaPlayerBridge::SetFdSource(const std::string& path) {
  int32_t fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(ERROR) << "SetFdSource error:Open file failed";
    return -1;
  }
  struct stat64 buffer;
  if (fstat64(fd, &buffer) != 0) {
    LOG(ERROR) << "SetFdSource error:Get file state failed";
    (void)close(fd);
    return -1;
  }
  int64_t length = static_cast<int64_t>(buffer.st_size);
  int32_t ret = player_->SetSource(fd, 0, length);
  (void)close(fd);
  return ret;
}

}  // namespace media

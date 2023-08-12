// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OHOS_MEDIA_PLAYER_BRIDGE_H_
#define MEDIA_BASE_OHOS_MEDIA_PLAYER_BRIDGE_H_

#include <player.h>
#include <surface.h>
#include <deque>

#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"
#include "media/base/ohos/ohos_media_player_callback.h"
#include "media/base/ohos/ohos_media_player_listener.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {
class MEDIA_EXPORT OHOSMediaPlayerBridge {
 public:
  class Client {
   public:
    virtual void OnFrameAvailable(int fd,
                                  uint32_t size,
                                  int32_t coded_width,
                                  int32_t coded_height,
                                  int32_t visible_width,
                                  int32_t visible_height,
                                  int32_t format) = 0;

    // Called when media duration is first detected or changes.
    virtual void OnMediaDurationChanged(base::TimeDelta duration) = 0;

    // Called when playback completed.
    virtual void OnPlaybackComplete() = 0;

    // Called when error happens.
    virtual void OnError(int error) = 0;

    // Called when video size has changed.
    virtual void OnVideoSizeChanged(int width, int height) = 0;
  };

  enum MediaErrorType {
    MEDIA_ERROR_FORMAT,
    MEDIA_ERROR_DECODE,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK,
    MEDIA_ERROR_INVALID_CODE,
    MEDIA_ERROR_SERVER_DIED,
  };

  OHOSMediaPlayerBridge(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        const std::string& user_agent,
                        bool hide_url_log,
                        Client* client,
                        bool allow_credentials,
                        bool is_hls);
  virtual ~OHOSMediaPlayerBridge();

  OHOSMediaPlayerBridge(const OHOSMediaPlayerBridge&) = delete;
  OHOSMediaPlayerBridge& operator=(const OHOSMediaPlayerBridge&) = delete;

  int32_t Initialize();
  void Start();
  void Pause();
  void SeekTo(base::TimeDelta time);
  base::TimeDelta GetDuration();
  void SetVolume(float volume);
  base::TimeDelta GetMediaTime();
  void FinishPaint(int fd);
  void SetPlaybackSpeed(OHOS::Media::PlaybackRateMode mode);

  void OnEnd();
  void OnError(int32_t errorCode);
  void OnBufferAvailable(OHOS::sptr<OHOS::SurfaceBuffer> buffer);
  void OnPlayerStateUpdate(OHOS::Media::PlayerStates player_state);

 private:
  int32_t SetFdSource(const std::string& path);
  void Prepare();
  void StartInternal();
  void SeekInternal(base::TimeDelta time);
  void PropagateDuration(base::TimeDelta duration);

  const std::string surfaceFormat = "SURFACE_FORMAT";
  std::shared_ptr<OHOS::Media::Player> player_ = nullptr;
  std::deque<OHOS::sptr<OHOS::SurfaceBuffer>> cached_buffers_;
  OHOS::sptr<OHOS::Surface> consumer_surface_ = nullptr;
  OHOS::sptr<OHOS::IBufferConsumerListener> listener_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Client* client_;
  GURL url_;
  bool prepared_;
  bool pending_play_;
  bool should_seek_on_prepare_;
  float volume_;
  bool should_set_volume_on_prepare_;
  base::TimeDelta duration_;
  base::TimeDelta pending_seek_;
  OHOS::Media::PlayerStates player_state_;

  // MediaPlayer is unable to handle Seek request when playback end. We should
  // pending the SeekTo request until its playback state changed.
  bool seeking_on_playback_complete_;

  base::WeakPtrFactory<OHOSMediaPlayerBridge> weak_factory_{this};
};
}  // namespace media

#endif  // MEDIA_BASE_OHOS_MEDIA_PLAYER_BRIDGE_H_

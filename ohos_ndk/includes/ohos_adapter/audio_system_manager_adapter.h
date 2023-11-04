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

#ifndef AUDIO_SYSTEM_MANAGER_ADAPTER_H
#define AUDIO_SYSTEM_MANAGER_ADAPTER_H

#include "audio_renderer_adapter.h"

namespace OHOS::NWeb {
enum class AudioAdapterStreamType {
    STREAM_DEFAULT = -1,
    STREAM_VOICE_CALL = 0,
    STREAM_MUSIC = 1,
    STREAM_RING = 2,
    STREAM_MEDIA = 3,
    STREAM_VOICE_ASSISTANT = 4,
    STREAM_SYSTEM = 5,
    STREAM_ALARM = 6,
    STREAM_NOTIFICATION = 7,
    STREAM_BLUETOOTH_SCO = 8,
    STREAM_ENFORCED_AUDIBLE = 9,
    STREAM_DTMF = 10,
    STREAM_TTS =  11,
    STREAM_ACCESSIBILITY = 12,
    STREAM_RECORDING = 13,
    STREAM_ALL = 100
};

struct AudioAdapterInterrupt {
    AudioAdapterStreamUsage streamUsage;
    AudioAdapterContentType contentType;
    AudioAdapterStreamType streamType;
    uint32_t sessionID;
    bool pauseWhenDucked;
};

class AudioManagerCallbackAdapter {
public:
    AudioManagerCallbackAdapter() = default;

    virtual ~AudioManagerCallbackAdapter() = default;

    virtual void OnSuspend() = 0;

    virtual void OnResume() = 0;
};

class AudioSystemManagerAdapter {
public:
    AudioSystemManagerAdapter() = default;

    virtual ~AudioSystemManagerAdapter() = default;

    virtual bool HasAudioOutputDevices() const = 0;

    virtual bool HasAudioInputDevices() const = 0;

    virtual int32_t RequestAudioFocus(const AudioAdapterInterrupt &audioInterrupt) = 0;

    virtual int32_t AbandonAudioFocus(const AudioAdapterInterrupt &audioInterrupt) = 0;

    virtual int32_t SetAudioManagerInterruptCallback(const std::shared_ptr<AudioManagerCallbackAdapter> &callback) = 0;

    virtual int32_t UnsetAudioManagerInterruptCallback() = 0;
};
} // namespace OHOS::NWeb

#endif // AUDIO_RENDERER_ADAPTER_H
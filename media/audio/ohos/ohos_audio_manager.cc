// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_manager.h"

#include "audio_system_manager_adapter.h"
#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "ohos_adapter_helper.h"

namespace media {

constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;
constexpr int kMinimumOutputBufferSize = 1024;
static const char* AUDIO_MANAGER_NAME = "OHOS";

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<OHOSAudioManager>(std::move(audio_thread),
                                            audio_log_factory);
}

OHOSAudioManager::OHOSAudioManager(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

OHOSAudioManager::~OHOSAudioManager() = default;

// Implementation of AudioManager.
bool OHOSAudioManager::HasAudioOutputDevices() {
  return OhosAdapterHelper::GetInstance()
      .GetAudioSystemManager()
      .HasAudioOutputDevices();
}

bool OHOSAudioManager::HasAudioInputDevices() {
  return OhosAdapterHelper::GetInstance()
      .GetAudioSystemManager()
      .HasAudioInputDevices();
}

void OHOSAudioManager::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    return;
  }
  *device_names = {AudioDeviceName::CreateDefault()};
}

const char* OHOSAudioManager::GetName() {
  return AUDIO_MANAGER_NAME;
}

// Implementation of AudioManagerBase.
AudioOutputStream* OHOSAudioManager::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* OHOSAudioManager::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return new OHOSAudioOutputStream(this, params);
}

AudioInputStream* OHOSAudioManager::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* OHOSAudioManager::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioParameters OHOSAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         GuessChannelLayout(kDefaultChannelCount),
                         kDefaultSampleRate, kMinimumOutputBufferSize);
}

}  // namespace media

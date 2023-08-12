// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_manager.h"

#include <stdlib.h>
#include "audio_system_manager_adapter.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "media/base/media_switches.h"
#include "ohos_adapter_helper.h"

namespace media {

constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;
constexpr int kMinimumOutputBufferSize = 2048;
constexpr int kMinimumInputBufferSize = 2048;
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
  auto audioDeviceList =
      OhosAdapterHelper::GetInstance().GetAudioSystemManager().GetDevices(
          AdapterDeviceFlag::OUTPUT_DEVICES_FLAG);
  for (auto audioDevice : audioDeviceList) {
    AudioDeviceName device;
    device.unique_id = std::to_string(audioDevice.deviceId);
    device.device_name = audioDevice.deviceName;
    device_names->push_back(device);
  }
}

void OHOSAudioManager::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioInput)) {
    return;
  }
  auto audioDeviceList =
      OhosAdapterHelper::GetInstance().GetAudioSystemManager().GetDevices(
          AdapterDeviceFlag::INPUT_DEVICES_FLAG);
  for (auto audioDevice : audioDeviceList) {
    AudioDeviceName device;
    device.unique_id = std::to_string(audioDevice.deviceId);
    device.device_name = audioDevice.deviceName;
    device_names->push_back(device);
  }
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
  LOG(INFO) << "OHOSAudioManager::MakeLowLatencyOutputStream";
  SelectAudioDevice(device_id, false);
  return new OHOSAudioOutputStream(this, params, isCommunication_);
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
  LOG(INFO) << "OHOSAudioManager::MakeLowLatencyInputStream";
  isCommunication_ = true;
  SelectAudioDevice(device_id, true);
  return new OHOSAudioInputStream(this, params);
}

AudioParameters OHOSAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  LOG(INFO) << "OHOSAudioManager::GetPreferredOutputStreamParameters";
  SelectAudioDevice(output_device_id, false);
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         GuessChannelLayout(kDefaultChannelCount),
                         kDefaultSampleRate, kMinimumOutputBufferSize);
}

AudioParameters OHOSAudioManager::GetPreferredInputStreamParameters(
    const std::string& input_device_id) {
  LOG(INFO) << "OHOSAudioManager::GetPreferredInputStreamParameters";
  AudioParameters params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      GuessChannelLayout(kDefaultChannelCount),
                      kDefaultSampleRate, kMinimumInputBufferSize);
  params.set_effects(AudioParameters::ECHO_CANCELLER |
                     AudioParameters::NOISE_SUPPRESSION |
                     AudioParameters::AUTOMATIC_GAIN_CONTROL);
  return params;
}

void OHOSAudioManager::ReleaseInputStream(AudioInputStream* stream) {
  AudioManagerBase::ReleaseInputStream(stream);
  isCommunication_ = false;
}

void OHOSAudioManager::SelectAudioDevice(const std::string& device_id,
                                         bool isInput) {
  LOG(INFO) << "OHOSAudioManager::SelectAudioDevice";
  AudioAdapterDeviceDesc desc;
  desc.deviceId = atoi(device_id.c_str());
  desc.deviceName = std::string();
  int32_t ret = OhosAdapterHelper::GetInstance()
                    .GetAudioSystemManager()
                    .SelectAudioDevice(desc, isInput);
  if (ret != 0)
    LOG(ERROR) << "OHOSAudioManager::SelectAudioDevice failed. ret: " << ret;
}

}  // namespace media

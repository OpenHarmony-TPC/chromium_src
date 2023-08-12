// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_output_stream.h"

#include "media/base/audio_timestamp_helper.h"
#include "ohos_adapter_helper.h"

namespace media {

OHOSAudioOutputStream::OHOSAudioOutputStream(OHOSAudioManager* manager,
                                             const AudioParameters& parameters)
    : manager_(manager),
      parameters_(parameters),
      audio_bus_(AudioBus::Create(parameters)) {
  audio_renderer_ =
      OhosAdapterHelper::GetInstance().CreateAudioRendererAdapter();
  sample_format_ = kSampleFormatS16;
  bytes_per_frame_ = parameters.GetBytesPerFrame(sample_format_);
  buffer_size_bytes_ = parameters.GetBytesPerBuffer(sample_format_);

  for (int i = 0; i < kMaxNumOfBuffersInQueue; i++) {
    audio_data_[i] = nullptr;
  }
}

OHOSAudioOutputStream::~OHOSAudioOutputStream() {
  // Close() must be called first.
  if (audio_renderer_ != nullptr) {
    (void)audio_renderer_->Release();
    audio_renderer_ = nullptr;
  }
}

bool OHOSAudioOutputStream::Open() {
  AudioAdapterRendererOptions rendererOptions;
  rendererOptions.samplingRate =
      static_cast<AudioAdapterSamplingRate>(parameters_.sample_rate());
  rendererOptions.encoding = AudioAdapterEncodingType::ENCODING_PCM;
  rendererOptions.format = AudioAdapterSampleFormat::SAMPLE_S16LE;
  rendererOptions.channels =
      static_cast<AudioAdapterChannel>(parameters_.channels());
  rendererOptions.contentType = AudioAdapterContentType::CONTENT_TYPE_MUSIC;
  rendererOptions.streamUsage = AudioAdapterStreamUsage::STREAM_USAGE_MEDIA;
  rendererOptions.rendererFlags = 0;

  if (!InitRender(rendererOptions)) {
    return false;
  }
  if (!SetupAudioBuffer()) {
    return false;
  }
  active_buffer_index_ = 0;
  return true;
}

void OHOSAudioOutputStream::Close() {
  Stop();
  ReleaseAudioBuffer();
  manager_->ReleaseOutputStream(this);
}

void OHOSAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(!callback_);
  DCHECK(reference_time_.is_null());
  DCHECK(!timer_.IsRunning());

  if (StartRender()) {
    callback_ = callback;
    memset(audio_data_[active_buffer_index_], 0, buffer_size_bytes_);
    active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
    if (callback_) {
      DCHECK(!timer_.IsRunning());
      PumpSamples();
    }
  }
}

void OHOSAudioOutputStream::Stop() {
  callback_ = nullptr;
  if (!reference_time_.is_null()) {
    reference_time_ = base::TimeTicks();
  }
  timer_.Stop();
  if (!audio_renderer_->Stop()) {
    ReportError();
  }
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void OHOSAudioOutputStream::Flush() {}

void OHOSAudioOutputStream::SetVolume(double volume) {
  DCHECK(0.0 <= volume && volume <= 1.0) << volume;
  (void)audio_renderer_->SetVolume(volume);
}

void OHOSAudioOutputStream::GetVolume(double* volume) {
  *volume = static_cast<double>(audio_renderer_->GetVolume());
}

base::TimeTicks OHOSAudioOutputStream::GetCurrentStreamTime() {
  DCHECK(!reference_time_.is_null());
  return reference_time_ +
         AudioTimestampHelper::FramesToTime(stream_position_samples_,
                                            parameters_.sample_rate());
}

bool OHOSAudioOutputStream::InitRender(
    const AudioAdapterRendererOptions& rendererOptions) {
  int32_t ret = audio_renderer_->Create(rendererOptions);
  if (ret != 0) {
    if (!audio_renderer_->Release()) {
      LOG(ERROR) << "ohos audio render release failed";
    }
    return false;
  }
  return true;
}

bool OHOSAudioOutputStream::StartRender() {
  if (!audio_renderer_->Start()) {
    LOG(ERROR) << "ohos audio render start failed";
    if (!audio_renderer_->Release()) {
      LOG(ERROR) << "ohos audio render release failed";
    }
    ReportError();
    return false;
  }
  return true;
}

void OHOSAudioOutputStream::ReportError() {
  LOG(ERROR) << "ohos audio render error happened";
  reference_time_ = base::TimeTicks();
  timer_.Stop();
  if (callback_)
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void OHOSAudioOutputStream::PumpSamples() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay;

  if (reference_time_.is_null()) {
    stream_position_samples_ = 0;
  } else {
    auto stream_time = GetCurrentStreamTime();
    // Adjust stream position if we missed timer deadline.
    if (now > stream_time + parameters_.GetBufferDuration() / 2) {
      stream_position_samples_ += AudioTimestampHelper::TimeToFrames(
          now - stream_time - parameters_.GetBufferDuration() / 2,
          parameters_.sample_rate());
    }

    uint64_t latency = 0;
    audio_renderer_->GetLatency(latency);
    delay = base::Microseconds(latency);
  }

  // Request more samples from |callback_|.
  int frames_filled = callback_->OnMoreData(delay, now, 0, audio_bus_.get());
  DCHECK_EQ(frames_filled, audio_bus_->frames());
  audio_bus_->Scale(volume_);
  audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
      frames_filled,
      reinterpret_cast<int16_t*>(audio_data_[active_buffer_index_]));
  const int num_filled_bytes = frames_filled * bytes_per_frame_;

  int32_t result = audio_renderer_->Write(audio_data_[active_buffer_index_],
                                          num_filled_bytes);
  if (result < 0) {
    ReportError();
  }

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;

  if (reference_time_.is_null()) {
    reference_time_ = now;
  }

  stream_position_samples_ += frames_filled;
  SchedulePumpSamples(now);
}

void OHOSAudioOutputStream::SchedulePumpSamples(base::TimeTicks now) {
  timer_.Start(FROM_HERE, GetCurrentStreamTime() - now,
               base::BindOnce(&OHOSAudioOutputStream::PumpSamples,
                              base::Unretained(this)));
}

bool OHOSAudioOutputStream::SetupAudioBuffer() {
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    audio_data_[i] = new uint8_t[buffer_size_bytes_];
    if (!audio_data_[i]) {
      LOG(ERROR) << "audio stream buffer setup failed";
      return false;
    }
  }
  return true;
}

void OHOSAudioOutputStream::ReleaseAudioBuffer() {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      if (audio_data_[i]) {
        delete[] audio_data_[i];
        audio_data_[i] = nullptr;
      }
    }
}
}  // namespace media

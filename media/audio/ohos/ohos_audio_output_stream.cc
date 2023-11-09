// Copyright (c) 2022 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_output_stream.h"

#include "base/bind.h"
#include "media/base/audio_timestamp_helper.h"
#include "ohos_adapter_helper.h"

namespace media {

AudioRendererWriteCallback::AudioRendererWriteCallback(const OnWriteDataCallback& callback)
  : callback_(callback) {}

AudioRendererWriteCallback::~AudioRendererWriteCallback() {}

void AudioRendererWriteCallback::OnWriteData(size_t length) {
  if (!callback_.is_null()) {
    callback_.Run();
  }
}

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
  DCHECK(reference_time_.is_null());

  SetAudioSourceCallback(callback);
  memset(audio_data_[active_buffer_index_], 0, buffer_size_bytes_);
  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
  if (!StartRender()) {
    LOG(ERROR) << "ohos audio start render failed";
  }
}

void OHOSAudioOutputStream::Stop() {
  ReleaseAudioSourceCallback();
  if (!reference_time_.is_null()) {
    reference_time_ = base::TimeTicks();
  }
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
  if (!audio_write_cb_) {
    audio_write_cb_ = std::make_shared<AudioRendererWriteCallback>(
      base::BindRepeating(&OHOSAudioOutputStream::PumpSamples, base::Unretained(this)));
    ret = audio_renderer_->SetAudioRendererWriteCallbackAdapter(audio_write_cb_);
    if (ret != 0) {
      LOG(ERROR) << "ohos audio set renferer write callback failed";
      return false;
    }
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
  std::lock_guard<std::mutex> lock(audio_mutex_);
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

  int frames_filled = 0;
  if (!OnMoreData(delay, now, 0, audio_bus_.get(), frames_filled)) {
    return;
  }
  DCHECK_EQ(frames_filled, audio_bus_->frames());
  audio_bus_->Scale(volume_);
  audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
      frames_filled,
      reinterpret_cast<int16_t*>(audio_data_[active_buffer_index_]));
  const int num_filled_bytes = frames_filled * bytes_per_frame_;
  BufferDescAdapter bufOutputInfo;
  int result = audio_renderer_->GetBufferDesc(bufOutputInfo);
  if (result != 0) {
    ReportError();
  }
  bufOutputInfo.buffer = audio_data_[active_buffer_index_];
  bufOutputInfo.bufLength = num_filled_bytes;
  bufOutputInfo.dataLength = bufOutputInfo.bufLength;
  result = audio_renderer_->Enqueue(bufOutputInfo);
  if (result != 0) {
    ReportError();
  }

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;

  if (reference_time_.is_null()) {
    reference_time_ = now;
  }

  stream_position_samples_ += frames_filled;
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

void OHOSAudioOutputStream::SetAudioSourceCallback(AudioSourceCallback* callback)
{
  std::lock_guard<std::mutex> lock(audio_mutex_);
  callback_ = callback;
}

bool OHOSAudioOutputStream::OnMoreData(base::TimeDelta delay,
                                       base::TimeTicks delay_timestamp,
                                       int prior_frames_skipped,
                                       AudioBus* dest,
                                       int& frames_filled) {
  std::lock_guard<std::mutex> lock(audio_mutex_);
  if (callback_) {
    frames_filled = callback_->OnMoreData(delay, delay_timestamp, 0, audio_bus_.get());
    return true;
  }
  return false;
}

void OHOSAudioOutputStream::ReleaseAudioSourceCallback() {
  std::lock_guard<std::mutex> lock(audio_mutex_);
  callback_ = nullptr;
}
}  // namespace media

// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_media_decoder_bridge_impl.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "base/logging.h"

using namespace media;
using namespace OHOS::NWeb;
using namespace std;

void clearIntqueue(std::queue<uint32_t>& q) {
  std::queue<uint32_t> empty;
  std::swap(empty, q);
}

void clearFlagqueue(std::queue<BufferFlag>& q) {
  std::queue<BufferFlag> empty;
  std::swap(empty, q);
}

void clearBufferqueue(std::queue<OhosBuffer>& q) {
  std::queue<OhosBuffer> empty;
  std::swap(empty, q);
}

void clearInfoqueue(std::queue<BufferInfo>& q) {
  std::queue<BufferInfo> empty;
  std::swap(empty, q);
}

VideoBridgeCodecConfig::VideoBridgeCodecConfig() = default;
VideoBridgeCodecConfig::~VideoBridgeCodecConfig() = default;

// static
std::unique_ptr<MediaCodecDecoderBridgeImpl>
MediaCodecDecoderBridgeImpl::CreateVideoDecoder(
    const VideoBridgeCodecConfig& config) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::CreateVideoDecoder.";
  std::string codec_type;
  if (config.codec == media::VideoCodec::kH264) {
    LOG(INFO) << "OhosVideoDecoder::CreateCodec video/avc";
    codec_type = "video/avc";
  } else if (config.codec == media::VideoCodec::kHEVC) {
    LOG(INFO) << "OhosVideoDecoder::CreateCodec video/hevc";
    codec_type = "video/hevc";
  } else {
    return nullptr;
  }
  return absl::WrapUnique(new MediaCodecDecoderBridgeImpl(
      codec_type, config.on_buffers_available_cb));
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByMime(
    std::string mimetype) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByMime.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  DecoderAdapterCode ret = videoDecoder_->CreateVideoDecoderByMime(mimetype);
  if (ret == DecoderAdapterCode::DECODER_ERROR) {
    LOG(ERROR) << "create decoder failed.";
    return ret;
  }

  if (signal_ == nullptr) {
    signal_ = make_shared<DecoderBridgeSignal>();
  }

  cb_ = make_unique<CodecBridgeCallback>(signal_);
  return videoDecoder_->SetCallbackDec(cb_);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::CreateVideoBridgeDecoderByName(
    std::string name) {
  LOG(INFO) << "create video decoder by name, type : " << name.c_str();
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  DecoderAdapterCode ret =
      videoDecoder_->CreateVideoDecoderByName(name.c_str());

  if (ret == DecoderAdapterCode::DECODER_ERROR) {
    LOG(ERROR) << "create decoder failed.";
    return ret;
  }

  if (signal_ == nullptr) {
    signal_ = make_shared<DecoderBridgeSignal>();
  }

  cb_ = make_shared<CodecBridgeCallback>(signal_);
  return videoDecoder_->SetCallbackDec(cb_);
}

MediaCodecDecoderBridgeImpl::MediaCodecDecoderBridgeImpl(
    std::string codec_type,
    base::RepeatingClosure on_buffers_available_cb) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::MediaCodecDecoderBridgeImpl().";
  if (!on_buffers_available_cb)
    return;
  videoDecoder_ =
      OhosAdapterHelper::GetInstance().CreateMediaCodecDecoderAdapter();
  DecoderAdapterCode ret = CreateVideoBridgeDecoderByMime(codec_type);
  if (ret == DecoderAdapterCode::DECODER_ERROR) {
    LOG(ERROR) << "create decoder failed.";
    return;
  }
  cb_->on_buffers_available_cb_ = on_buffers_available_cb;
}

MediaCodecDecoderBridgeImpl::~MediaCodecDecoderBridgeImpl() {
  ReleaseBridgeDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder(
    int32_t width,
    int32_t height,
    double framerate) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder configure "
               "decoder.";
  width_ = width;
  height_ = height;
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::ConfigureBridgeDecoder decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->ConfigureDecoder(width, height, framerate);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder(
    const DecoderFormat& format) {
  LOG(INFO) << " MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder set "
               "decoder parameter.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::SetBridgeParameterDecoder "
                  "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->SetParameterDecoder(format);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface(
    void* window) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface set "
               "decoder outputsurface.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::SetBridgeOutputSurface decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->SetOutputSurface(window);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::GetOutputFormatBridgeDecoder(
    int32_t& width,
    int32_t& height) {
  LOG(DEBUG) << "MediaCodecDecoderBridgeImpl::GetOutputFormatBridgeDecoder get "
                "decoder outputformat.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "MediaCodecDecoderBridgeImpl::GetOutputFormatBridgeDecoder "
                  "decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->GetOutputFormatDec(width, height);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PrepareBridgeDecoder() {
  LOG(INFO)
      << "MediaCodecDecoderBridgeImpl::PrepareBridgeDecoder prepare decoder.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << " MediaCodecDecoderBridgeImpl::PrepareBridgeDecoder decoder "
                  "is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->PrepareDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::StartBridgeDecoder() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::StartBridgeDecoder start decoder.";
  isRunning_.store(true);

  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::StartBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  return videoDecoder_->StartDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::StopBridgeDecoder() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::StopBridgeDecoder stop decoder.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::StopBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->StopDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::FlushBridgeDecoder() {
  LOG(INFO)
      << " MediaCodecDecoderBridgeImpl::FlushBridgeDecoder flush decoder.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::FlushBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  unique_lock<mutex> lock(signal_->inMutex_);
  unique_lock<mutex> lock2(signal_->outMutex_);
  signal_->isDecoderFlushing_.store(true);
  lock.unlock();
  lock2.unlock();
  DecoderAdapterCode ret = videoDecoder_->FlushDecoder();
  if (ret != DecoderAdapterCode::DECODER_OK) {
    LOG(ERROR) << " MediaCodecDecoderBridgeImpl::FlushBridgeDecoder flush "
                  "decoder failed.";
    return ret;
  }
  unique_lock<mutex> lockIn(signal_->inMutex_);
  clearIntqueue(signal_->inQueue_);
  clearBufferqueue(signal_->inBufferQueue_);
  lockIn.unlock();

  unique_lock<mutex> lockOut(signal_->outMutex_);
  clearIntqueue(signal_->outQueue_);
  clearFlagqueue(signal_->flagQueue_);
  clearInfoqueue(signal_->outBufferInfoQueue_);
  lockOut.unlock();

  signal_->isDecoderFlushing_.store(false);
  return videoDecoder_->StartDecoder();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ResetBridgeDecoder() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::ResetBridgeDecoder reset decoder.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::ResetBridgeDecoder decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }

  unique_lock<mutex> lock(signal_->inMutex_);
  unique_lock<mutex> lock2(signal_->outMutex_);
  signal_->isDecoderFlushing_.store(true);
  lock.unlock();
  lock2.unlock();

  DecoderAdapterCode ret = videoDecoder_->ResetDecoder();

  unique_lock<mutex> lockIn(signal_->inMutex_);
  clearIntqueue(signal_->inQueue_);
  clearBufferqueue(signal_->inBufferQueue_);
  lockIn.unlock();

  unique_lock<mutex> lockOut(signal_->outMutex_);
  clearIntqueue(signal_->outQueue_);
  clearFlagqueue(signal_->flagQueue_);
  clearInfoqueue(signal_->outBufferInfoQueue_);
  lockOut.unlock();

  signal_->isDecoderFlushing_.store(false);
  LOG(DEBUG) << "reset decoder success.";
  return ret;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ReleaseBridgeDecoder() {
  LOG(INFO) << "release decoder.";
  isRunning_.store(false);

  if (videoDecoder_ != nullptr) {
    videoDecoder_->ReleaseDecoder();
    videoDecoder_ = nullptr;
  }

  LOG(DEBUG) << "release decoder success.";
  return DecoderAdapterCode::DECODER_OK;
}

void MediaCodecDecoderBridgeImpl::PopInqueueDec() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::PopInqueueDec";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR) << "decoder is NULL.";
    return;
  }
  signal_->inQueue_.pop();
  signal_->inBufferQueue_.pop();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PushInbufferDec(
    const uint32_t index,
    const uint32_t& bufferSize,
    const int64_t& time) {
  BufferInfo bufferInfo;
  bufferInfo.presentationTimeUs = time;
  bufferInfo.size = bufferSize;
  bufferInfo.offset = 0;

  BufferFlag bufferFlag;
  if (isFirstDecFrame_) {
    bufferFlag = BufferFlag::CODEC_BUFFER_FLAG_CODEC_DATA;
    isFirstDecFrame_ = false;
  } else {
    bufferFlag = BufferFlag::CODEC_BUFFER_FLAG_NONE;
  }
  LOG(DEBUG) << "PushInbufferDec index:" << index
             << ", buffersize:" << bufferSize;
  return videoDecoder_->QueueInputBufferDec(index, bufferInfo, bufferFlag);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::PushInbufferDecEos(
    const uint32_t index) {
  BufferInfo bufferInfo;
  bufferInfo.presentationTimeUs = 0;
  bufferInfo.size = 0;
  bufferInfo.offset = 0;

  BufferFlag bufferFlag;
  bufferFlag = BufferFlag::CODEC_BUFFER_FLAG_EOS;
  return videoDecoder_->QueueInputBufferDec(index, bufferInfo, bufferFlag);
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::QueueInputBuffer(
    const uint8_t* data,
    size_t data_size,
    int64_t presentation_time) {
  LOG(DEBUG) << "MediaCodecDecoderBridgeImpl::QueueInputBuffer";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::QueueInputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  unique_lock<mutex> lock(signal_->inMutex_);
  if (signal_->isOnError_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->inBufferQueue_.empty()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  uint32_t index = signal_->inQueue_.front();
  OhosBuffer buffer = signal_->inBufferQueue_.front();
  uint32_t bufferSize = buffer.bufferSize;

  size_t inputSize = bufferSize >= data_size ? data_size : bufferSize;
  LOG(DEBUG) << "MediaCodecDecoderBridgeImpl::QueueInputBuffer bufferSize: "
             << bufferSize << " " << data_size;
  memcpy(buffer.addr, data, inputSize);
  DecoderAdapterCode ret = PushInbufferDec(index, inputSize, presentation_time);

  PopInqueueDec();
  return ret;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::QueueInputBufferEOS() {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::QueueInputBufferEOS";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::QueueInputBufferEOS decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  unique_lock<mutex> lock(signal_->inMutex_);
  if (signal_->isOnError_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->inBufferQueue_.empty()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  uint32_t index = signal_->inQueue_.front();
  DecoderAdapterCode ret = PushInbufferDecEos(index);

  PopInqueueDec();
  return ret;
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::ReleaseOutputBuffer(
    uint32_t index,
    bool render) {
  LOG(INFO) << "MediaCodecDecoderBridgeImpl::ReleaseOutputBuffer.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::ReleaseOutputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  return videoDecoder_->ReleaseOutputBufferDec(index, render);
}

void MediaCodecDecoderBridgeImpl::PopOutqueueDec() {
  LOG(DEBUG) << "MediaCodecDecoderBridgeImpl::PopOutqueueDec.";
  signal_->outQueue_.pop();
  signal_->flagQueue_.pop();
  signal_->outBufferInfoQueue_.pop();
}

DecoderAdapterCode MediaCodecDecoderBridgeImpl::DequeueOutputBuffer(
    base::TimeDelta* presentation_time,
    uint32_t& index,
    bool& eos) {
  LOG(DEBUG) << "MediaCodecDecoderBridgeImpl::DequeueOutputBuffer.";
  if (videoDecoder_ == nullptr) {
    LOG(ERROR)
        << "MediaCodecDecoderBridgeImpl::DequeueOutputBuffer decoder is NULL.";
    return DecoderAdapterCode::DECODER_ERROR;
  }
  unique_lock<mutex> lock(signal_->outMutex_);
  if (signal_->isOnError_) {
    return DecoderAdapterCode::DECODER_ERROR;
  }
  if (signal_->outQueue_.empty()) {
    return DecoderAdapterCode::DECODER_RETRY;
  }
  index = signal_->outQueue_.front();
  eos = signal_->flagQueue_.front() == BufferFlag::CODEC_BUFFER_FLAG_EOS;
  *presentation_time = base::Microseconds(
      signal_->outBufferInfoQueue_.front().presentationTimeUs);

  PopOutqueueDec();
  return DecoderAdapterCode::DECODER_OK;
}

void MediaCodecDecoderBridgeImpl::DestoryNativeWindow(void* window) {
  if (window) {
    OhosAdapterHelper::GetInstance().GetWindowAdapterInstance()
                                    .DestroyNativeWindow(window);
  }
}

void CodecBridgeCallback::OnError(ErrorType errorType, int32_t errorCode) {
  LOG(ERROR) << "CodecBridgeCallback::OnError Error errorCode=" << errorCode;
  unique_lock<mutex> lock(signal_->inMutex_);
  unique_lock<mutex> lock2(signal_->outMutex_);
  signal_->isOnError_ = true;

  clearIntqueue(signal_->inQueue_);
  clearBufferqueue(signal_->inBufferQueue_);

  clearIntqueue(signal_->outQueue_);
  clearFlagqueue(signal_->flagQueue_);
  clearInfoqueue(signal_->outBufferInfoQueue_);

  lock.unlock();
  lock2.unlock();
}

void CodecBridgeCallback::OnStreamChanged(const DecoderFormat& format) {
  LOG(INFO) << "CodecBridgeCallback::OnStreamChanged Output Format Changed.";
}

void CodecBridgeCallback::OnNeedInputData(uint32_t index, OhosBuffer buffer) {
  LOG(DEBUG)
      << "CodecBridgeCallback::OnNeedInputData Input Buffer Available, index = "
      << index;
  unique_lock<mutex> lock(signal_->inMutex_);
  if (signal_->isDecoderFlushing_.load()) {
    LOG(INFO) << "CodecBridgeCallback::OnNeedInputData Decoder is flushing.";
    return;
  }
  signal_->inQueue_.push(index);
  signal_->inBufferQueue_.push(buffer);
  on_buffers_available_cb_.Run();
}

void CodecBridgeCallback::OnNeedOutputData(uint32_t index,
                                           BufferInfo info,
                                           BufferFlag flag) {
  LOG(DEBUG) << "CodecBridgeCallback::OnNeedOutputData";
  unique_lock<mutex> lock(signal_->outMutex_);
  if (signal_->isDecoderFlushing_.load()) {
    LOG(INFO) << "CodecBridgeCallback::OnNeedOutputData Decoder is flushing.";
    return;
  }

  LOG(DEBUG) << "CodecBridgeCallback::OnNeedOutputData Output Buffer "
                "Available, index ="
             << index << ", timestamp = " << info.presentationTimeUs;
  signal_->outQueue_.push(index);
  signal_->flagQueue_.push(flag);
  signal_->outBufferInfoQueue_.push(info);
  on_buffers_available_cb_.Run();
}
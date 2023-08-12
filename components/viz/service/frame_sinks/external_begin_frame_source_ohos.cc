#include "components/viz/service/frame_sinks/external_begin_frame_source_ohos.h"
#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ohos_adapter_helper.h"

namespace viz {
using namespace OHOS::NWeb;

constexpr int VSYNC_PERIOD_MICROSECONDS = 16666;

class ExternalBeginFrameSourceOHOS::VSyncUserData {
 public:
  VSyncUserData(const scoped_refptr<base::SingleThreadTaskRunner>& current,
                viz::ExternalBeginFrameSourceOHOS* weakPtr)
      : current_(current), weakPtr_(weakPtr){};
  VSyncUserData(const VSyncUserData&) = delete;
  VSyncUserData& operator=(const VSyncUserData&) = delete;
  ~VSyncUserData() = default;

  const scoped_refptr<base::SingleThreadTaskRunner>& current_;
  base::WeakPtrFactory<viz::ExternalBeginFrameSourceOHOS> weakPtr_;
};

ExternalBeginFrameSourceOHOS::ExternalBeginFrameSourceOHOS(uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      vsync_notification_enabled_(false),
      user_data_(
          std::make_unique<VSyncUserData>(base::ThreadTaskRunnerHandle::Get(),
                                          this)),
      vsync_adapter_(OhosAdapterHelper::GetInstance().GetVSyncAdapter()) {
  TRACE_EVENT0("viz",
               "ExternalBeginFrameSourceOHOS::ExternalBeginFrameSourceOHOS");
}

ExternalBeginFrameSourceOHOS::~ExternalBeginFrameSourceOHOS() {
  SetEnabled(false);
}

void ExternalBeginFrameSourceOHOS::SetDynamicBeginFrameDeadlineOffsetSource(
    DynamicBeginFrameDeadlineOffsetSource*
        dynamic_begin_frame_deadline_offset_source) {
  TRACE_EVENT0(
      "viz",
      "ExternalBeginFrameSourceOHOS::SetDynamicBeginFrameDeadlineOffsetSource");
  begin_frame_args_generator_.set_dynamic_begin_frame_deadline_offset_source(
      dynamic_begin_frame_deadline_offset_source);
}

void ExternalBeginFrameSourceOHOS::OnVSync(int64_t timestamp, void* data) {
  if (data == nullptr) {
    LOG(ERROR) << "OnVSync data is nullptr";
    return;
  }
  VSyncUserData* userData = reinterpret_cast<VSyncUserData*>(data);
  if (!userData->current_) {
    LOG(ERROR) << "OnVSync data current is nullptr";
    return;
  }

  userData->current_->PostTask(
      FROM_HERE, base::BindOnce(&ExternalBeginFrameSourceOHOS::OnVSyncImpl,
                                userData->weakPtr_.GetWeakPtr(), timestamp));
}

void ExternalBeginFrameSourceOHOS::OnVSyncImpl(int64_t timestamp) {
  if (!vsync_notification_enabled_) {
    return;
  }

  base::TimeDelta vsync_period(base::Microseconds(VSYNC_PERIOD_MICROSECONDS));
  base::TimeTicks frame_time = base::TimeTicks() + base::Nanoseconds(timestamp);
  base::TimeTicks deadline = frame_time + vsync_period;
  TRACE_EVENT2("viz", "ExternalBeginFrameSourceOHOS::OnVSyncImpl", "frame_time",
               frame_time, "deadline", deadline);
  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, deadline, vsync_period);
  OnBeginFrame(begin_frame_args);

  vsync_adapter_->RequestVsync(reinterpret_cast<void*>(user_data_.get()),
                               ExternalBeginFrameSourceOHOS::OnVSync);
}

void ExternalBeginFrameSourceOHOS::OnNeedsBeginFrames(bool needs_begin_frames) {
  SetEnabled(needs_begin_frames);
}

void ExternalBeginFrameSourceOHOS::SetEnabled(bool enabled) {
  if (vsync_notification_enabled_ == enabled) {
    return;
  }
  TRACE_EVENT1("viz", "ExternalBeginFrameSourceOHOS::SetEnabled", "enabled",
               enabled);
  vsync_notification_enabled_ = enabled;
  if (vsync_notification_enabled_) {
    vsync_adapter_->RequestVsync(reinterpret_cast<void*>(user_data_.get()),
                                 ExternalBeginFrameSourceOHOS::OnVSync);
  }
}
}  // namespace viz
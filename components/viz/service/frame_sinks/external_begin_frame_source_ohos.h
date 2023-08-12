#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_OHOS_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_OHOS_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"
#include "graphic_adapter.h"

namespace viz {
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceOHOS
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  explicit ExternalBeginFrameSourceOHOS(uint32_t restart_id);
  ~ExternalBeginFrameSourceOHOS() override;
  ExternalBeginFrameSourceOHOS(const ExternalBeginFrameSourceOHOS&) = delete;
  ExternalBeginFrameSourceOHOS& operator=(const ExternalBeginFrameSourceOHOS&) =
      delete;

  // BeginFrameSource:
  void SetDynamicBeginFrameDeadlineOffsetSource(
      DynamicBeginFrameDeadlineOffsetSource*
          dynamic_begin_frame_deadline_offset_source) override;

  static void OnVSync(int64_t timestamp, void* data);
  void OnVSyncImpl(int64_t timestamp);

 private:
  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SetEnabled(bool enabled);

  BeginFrameArgsGenerator begin_frame_args_generator_;
  bool vsync_notification_enabled_;

  class VSyncUserData;
  std::unique_ptr<VSyncUserData> user_data_;
  std::unique_ptr<OHOS::NWeb::VSyncAdapter> vsync_adapter_;
};
}  // namespace viz

#endif
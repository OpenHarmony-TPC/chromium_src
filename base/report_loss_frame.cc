#include "base/report_loss_frame.h"
#include <chrono>
#include "ohos_nweb/src/sysevent/event_reporter.h"

constexpr double VSYNC_PERIOD = 16.6;
constexpr int JANK_STATS_VER = 1;

ReportLossFrame* ReportLossFrame::instance = nullptr;

ReportLossFrame::ReportLossFrame() {}

ReportLossFrame* ReportLossFrame::GetInstance() {
  if (instance == nullptr) {
    return new ReportLossFrame();
  }
  return instance;
}

ReportLossFrame::~ReportLossFrame() {
  instance = nullptr;
}

void ReportLossFrame::SetScrollState(ScrollMode state) {
  scroll_state_ = state;
}

void ReportLossFrame::SetPageUrl(std::string url) {
  page_url_ = url;
}

int64_t ReportLossFrame::GetCurrentTimestampMS() {
  auto currentTime = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime)
      .count();
}

void ReportLossFrame::Report() {
  int64_t now = GetCurrentTimestampMS();
  int duration = start_time_for_scroll - now;
  ReportJankStats(start_time_for_scroll, duration, page_url_, jank_stats,
                  JANK_STATS_VER);
  start_time_for_scroll = 0;
}

void ReportLossFrame::Record() {
  if (scroll_state_ == ScrollMode::STOP) {
    return;
  }

  if (start_time_for_scroll == 0) {
    start_time_for_scroll = GetCurrentTimestampMS();
    start_time_ = GetCurrentTimestampMS();
    return;
  }

  int64_t now = GetCurrentTimestampMS();
  int duration = start_time_ - now;
  start_time_for_scroll = now;

  if (duration <= VSYNC_PERIOD) {
    return;
  }

  double loss_frame = duration / VSYNC_PERIOD;

  size_t type = JANK_FREQ_EXCEED_FRAME;
  if (loss_frame < 6) {
    type = JANK_FREQ_6_FRAME;
  } else if (loss_frame < 15) {
    type = JANK_FREQ_15_FRAME;
  } else if (loss_frame < 20) {
    type = JANK_FREQ_20_FRAME;
  } else if (loss_frame < 36) {
    type = JANK_FREQ_36_FRAME;
  } else if (loss_frame < 48) {
    type = JANK_FREQ_48_FRAME;
  } else if (loss_frame < 60) {
    type = JANK_FREQ_60_FRAME;
  } else if (loss_frame < 120) {
    type = JANK_FREQ_120_FRAME;
  } else if (loss_frame < 180) {
    type = JANK_FREQ_180_FRAME;
  }

  jank_stats[type]++;
}
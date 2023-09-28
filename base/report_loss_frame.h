
#ifndef BASE_REPORT_LOSS_FRAME_H_
#define BASE_REPORT_LOSS_FRAME_H_

#include <vector>

enum ScrollMode {
    START,
    STOP
};

enum JankRangeType : size_t {
    JANK_FREQ_6_FRAME = 0,
    JANK_FREQ_15_FRAME,
    JANK_FREQ_20_FRAME,
    JANK_FREQ_36_FRAME,
    JANK_FREQ_48_FRAME,
    JANK_FREQ_60_FRAME,
    JANK_FREQ_120_FRAME,
    JANK_FREQ_180_FRAME,
    JANK_FREQ_EXCEED_FRAME
};

class ReportLossFrame {
    public:
        static ReportLossFrame* GetInstance();

        void Record();

        void Report();

        void SetScrollState(ScrollMode state);

        void SetPageUrl(std::string url);

        int64_t GetCurrentTimestampMS();

    private:

        ReportLossFrame();
        ~ReportLossFrame();

        ReportLossFrame(const ReportLossFrame&) = delete;
        ReportLossFrame& operator=(const ReportLossFrame&) = delete;

        ScrollMode scroll_state_ = ScrollMode::STOP;

        int64_t start_time_for_scroll = 0;

        int64_t start_time_;

        std::string page_url_;

        std::vector<uint16_t> jank_stats = std::vector<uint16_t>(0, 8);

        static ReportLossFrame* instance;
};

#endif // BASE_REPORT_LOSS_FRAME_H_
#include "ui/events/native_scroller_ohos.h"
#include <cmath>
#include "base/logging.h"

namespace ui {
namespace {
//constexpr float kUnitConvert = 1000.0f;
constexpr float kDefaultFriction = 0.6f;
constexpr float kFrictionScale = -4.2f;
constexpr float kDefaultThreshold = 0.75f;
constexpr float kDefaultMultiplier = 60.0f;
constexpr float kThresholdForFlingEnd = 0.001f;

inline bool NearEqual(const double left,
                      const double right,
                      const double epsilon) {
    return (std::abs(left - right) <= epsilon);
}

inline bool NearZero(const double value, const double epsilon) {
    return NearEqual(value, 0.0, epsilon);
}

inline bool NearZero(const double left) {
    constexpr double epsilon = 0.001f;
    return NearZero(left, epsilon);
}

inline bool GreatNotEqual(double left, double right) {
    constexpr double epsilon = 0.001f;
    return (left - right) > epsilon;
}
} // namespace

void NativeScrollerOhos::Fling(float start_x,
                               float start_y,
                               float velocity_x,
                               float velocity_y,
                               float min_x,
                               float max_x,
                               float min_y,
                               float max_y,
                               base::TimeTicks start_time) {
    curr_time_ = start_time;
    start_time_ = start_time;
    friction_ = kDefaultFriction * kFrictionScale;

    // currently only vertical fling is supported
    init_velocity_y_ = std::abs(velocity_y);
    init_y_ = start_y;

    value_threshold_ = kDefaultThreshold;
    velocity_threshold_ = value_threshold_ * kDefaultMultiplier;

    float diff_time = 0;
    if (NearZero(init_velocity_y_)) {
        signum_ = 0;
        diff_time = 0.0;
    } else {
        signum_ = GreatNotEqual(velocity_y, 0.0) ? 1.0 : -1.0;
        diff_time = std::log(velocity_threshold_ / init_velocity_y_) / friction_;
    }
    final_y_ = GetPosition(diff_time);
    finisheed_ = false;
}

bool NativeScrollerOhos::ComputeScrollOffset(base::TimeTicks time,
                                             gfx::Vector2dF* offset,
                                             gfx::Vector2dF* velocity) {
    if (finisheed_) {
        *offset = gfx::Vector2dF(0, final_y_);
        *velocity = gfx::Vector2dF();
        return false;
    }

    float diff_time = (time - start_time_).InSecondsF();
    last_y_ = curr_y_;
    curr_y_ = GetPosition(diff_time);
    curr_velocity_y_ = GetVelocity(diff_time);

    *offset = gfx::Vector2dF(0, curr_y_);
    *velocity = gfx::Vector2dF(0, curr_velocity_y_);

    if (ShouldAbortAnimation()) {
        AbortAnimation();
    }
    return !finisheed_;
}

bool NativeScrollerOhos::ShouldAbortAnimation() {
    return std::abs(curr_y_ - final_y_) < kThresholdForFlingEnd || 
           std::abs(curr_y_ - last_y_) <= kThresholdForFlingEnd;
}

double NativeScrollerOhos::GetPosition(float offsetTime /*second*/) {
    double position = init_y_ + signum_ * (init_velocity_y_ / friction_) * std::expm1(friction_ * offsetTime);
    return position;
}

double NativeScrollerOhos::GetVelocity(float offsetTime /*second*/) {
    auto velocity = signum_ * init_velocity_y_ * std::exp(friction_ * offsetTime);
    return velocity;
}

void NativeScrollerOhos::AbortAnimation() {
    finisheed_ = true;
}

}
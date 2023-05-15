#include "eta_estimator.h"

ETAEstimator::ETAEstimator(
        size_t total_events
)
        : total_events_(total_events), start_time_(std::chrono::steady_clock::time_point::min()) {}

void ETAEstimator::start() {
    start_time_ = std::chrono::steady_clock::now();
}

std::string ETAEstimator::estimate_time_remaining(
        size_t events_done
) {
    if (events_done == 0) {
        return "INF";
    }
    if (start_time_ == std::chrono::steady_clock::time_point::min()) {
        return "NOT_STARTED";
    }

    auto current_time = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_);
    double avg_time_per_event = static_cast<double>(time_elapsed.count()) / events_done;
    auto time_remaining = static_cast<size_t>((total_events_ - events_done) * avg_time_per_event);

    auto seconds_remaining = time_remaining / 1000;
    auto minutes_remaining = seconds_remaining / 60;
    auto hours_remaining = minutes_remaining / 60;
    auto days_remaining = hours_remaining / 24;

    seconds_remaining %= 60;
    minutes_remaining %= 60;
    hours_remaining %= 24;

    std::ostringstream ss;
    ss << "Processed: (" << events_done << '/' << total_events_ << "), ETA: " << days_remaining << "::" << std::setw(2) << std::setfill('0') << hours_remaining << ':'
       << std::setw(2) << std::setfill('0') << minutes_remaining << ':'
       << std::setw(2) << std::setfill('0') << seconds_remaining << " [days::h:m:s]";

    return ss.str();
}

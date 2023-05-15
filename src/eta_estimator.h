#pragma once

#include <iostream>
#include <chrono>
#include <iomanip>
#include <string>

class ETAEstimator {
public:
    explicit ETAEstimator(size_t total_events);

    void start();

    std::string estimate_time_remaining(size_t events_done);

private:
    size_t total_events_;
    std::chrono::steady_clock::time_point start_time_;
};

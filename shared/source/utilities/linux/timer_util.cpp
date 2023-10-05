/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/timer_util.h"

#include <chrono>

namespace NEO {

class Timer::TimerImpl {

  public:
    TimerImpl() {
    }

    ~TimerImpl() {
    }

    void start() {
        *((std::chrono::high_resolution_clock::time_point *)&startTime) = std::chrono::high_resolution_clock::now();
    }

    void end() {
        *((std::chrono::high_resolution_clock::time_point *)&endTime) = std::chrono::high_resolution_clock::now();
    }

    long long int get() {
        long long int nanosecondTime = 0;
        std::chrono::duration<double> diffTime = std::chrono::duration_cast<std::chrono::duration<double>>(*(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&endTime)) - *(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&startTime)));
        nanosecondTime = (long long int)(diffTime.count() * (double)1000000000.0);
        return nanosecondTime;
    }

    long long getStart() {
        long long ret = (long long)(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&startTime)->time_since_epoch().count());
        return ret;
    }

    long long getEnd() {
        long long ret = (long long)(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&endTime)->time_since_epoch().count());
        return ret;
    }

    TimerImpl &operator=(const TimerImpl &t) {
        if (this != &t) {
            startTime = t.startTime;
        }
        return *this;
    }

    static void setFreq() {
    }

  private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
};

Timer::Timer() {
    timerImpl = new TimerImpl();
}

Timer::~Timer() {
    delete timerImpl;
}

void Timer::start() {
    timerImpl->start();
}

void Timer::end() {
    timerImpl->end();
}

long long int Timer::get() {
    return timerImpl->get();
}

long long Timer::getStart() {
    return timerImpl->getStart();
}

long long Timer::getEnd() {
    return timerImpl->getEnd();
}

Timer &Timer::operator=(const Timer &t) {
    *timerImpl = *(t.timerImpl);
    return *this;
}

void Timer::setFreq() {
    TimerImpl::setFreq();
}
}; // namespace NEO

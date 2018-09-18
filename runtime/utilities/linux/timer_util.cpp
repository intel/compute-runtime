/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <chrono>
#include "runtime/utilities/timer_util.h"

namespace OCLRT {

class Timer::TimerImpl {

  public:
    TimerImpl() {
    }

    ~TimerImpl() {
    }

    void start() {
        *((std::chrono::high_resolution_clock::time_point *)&m_startTime) = std::chrono::high_resolution_clock::now();
    }

    void end() {
        *((std::chrono::high_resolution_clock::time_point *)&m_endTime) = std::chrono::high_resolution_clock::now();
    }

    long long int get() {
        long long int nanosecondTime = 0;
        std::chrono::duration<double> diffTime = std::chrono::duration_cast<std::chrono::duration<double>>(*(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&m_endTime)) - *(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&m_startTime)));
        nanosecondTime = (long long int)(diffTime.count() * (double)1000000000.0);
        return nanosecondTime;
    }

    long long getStart() {
        long long ret = (long long)(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&m_startTime)->time_since_epoch().count());
        return ret;
    }

    long long getEnd() {
        long long ret = (long long)(reinterpret_cast<std::chrono::high_resolution_clock::time_point *>(&m_endTime)->time_since_epoch().count());
        return ret;
    }

    TimerImpl &operator=(const TimerImpl &t) {
        m_startTime = t.m_startTime;
        return *this;
    }

    static void setFreq() {
    }

  private:
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
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
}; // namespace OCLRT

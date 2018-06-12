/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

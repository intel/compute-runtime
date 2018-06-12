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

#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/utilities/timer_util.h"

namespace OCLRT {

class Timer::TimerImpl {
  public:
    TimerImpl() {
        memset(&m_startTime, 0, sizeof(LARGE_INTEGER));
        memset(&m_endTime, 0, sizeof(LARGE_INTEGER));
    }

    ~TimerImpl() {
    }

    LARGE_INTEGER start() {
        QueryPerformanceCounter((LARGE_INTEGER *)&m_startTime);
        return m_startTime;
    }

    LARGE_INTEGER end() {
        QueryPerformanceCounter((LARGE_INTEGER *)&m_endTime);
        return m_endTime;
    }

    long long int get() {
        long long int nanosecondTime = 0;
        auto timeDelta = (double)(m_endTime.QuadPart - m_startTime.QuadPart);
        timeDelta /= (double)mFrequency.QuadPart;
        timeDelta *= 1000000000.0;

        if (m_endTime.QuadPart < m_startTime.QuadPart) {
            DEBUG_BREAK_IF(true);
        }
        return (long long)timeDelta;
    }

    long long getStart() {
        return (long long)(((LARGE_INTEGER *)&m_startTime)->QuadPart);
    }

    long long getEnd() {

        return (long long)(((LARGE_INTEGER *)&m_endTime)->QuadPart);
    }

    TimerImpl &operator=(const TimerImpl &t) {
        m_startTime = t.m_startTime;
        return *this;
    }

    static void setFreq() {
        QueryPerformanceFrequency(&mFrequency);
    }

  public:
    static LARGE_INTEGER mFrequency;

  private:
    LARGE_INTEGER m_startTime;
    LARGE_INTEGER m_endTime;
};

LARGE_INTEGER Timer::TimerImpl::mFrequency = {
    {{0}},
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

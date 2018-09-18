/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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

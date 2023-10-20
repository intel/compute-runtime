/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/timer_util.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

class Timer::TimerImpl {
  public:
    TimerImpl() {
        memset(&startTime, 0, sizeof(LARGE_INTEGER));
        memset(&endTime, 0, sizeof(LARGE_INTEGER));
    }

    ~TimerImpl() {
    }

    LARGE_INTEGER start() {
        QueryPerformanceCounter((LARGE_INTEGER *)&startTime);
        return startTime;
    }

    LARGE_INTEGER end() {
        QueryPerformanceCounter((LARGE_INTEGER *)&endTime);
        return endTime;
    }

    long long int get() {
        auto timeDelta = (double)(endTime.QuadPart - startTime.QuadPart);
        timeDelta /= (double)mFrequency.QuadPart;
        timeDelta *= 1000000000.0;

        if (endTime.QuadPart < startTime.QuadPart) {
            DEBUG_BREAK_IF(true);
        }
        return (long long)timeDelta;
    }

    long long getStart() {
        return (long long)(((LARGE_INTEGER *)&startTime)->QuadPart);
    }

    long long getEnd() {

        return (long long)(((LARGE_INTEGER *)&endTime)->QuadPart);
    }

    TimerImpl &operator=(const TimerImpl &t) {
        if (this == &t) {
            return *this;
        }
        startTime = t.startTime;
        return *this;
    }

    static void setFreq() {
        QueryPerformanceFrequency(&mFrequency);
    }

  public:
    static LARGE_INTEGER mFrequency;

  private:
    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
};

LARGE_INTEGER Timer::TimerImpl::mFrequency = {
    {},
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

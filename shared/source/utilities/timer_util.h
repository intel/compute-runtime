/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

namespace NEO {

class Timer : NEO::NonCopyableAndNonMovableClass {
  public:
    Timer();
    Timer &operator=(Timer &&) noexcept = delete;

    ~Timer();

    void start();

    void end();

    long long int get();

    long long getStart();
    long long getEnd();

    Timer &operator=(const Timer &t);

    static void setFreq();

  private:
    class TimerImpl;
    TimerImpl *timerImpl;
};

static_assert(NEO::NonMovable<Timer>);

}; // namespace NEO

/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <type_traits>

namespace NEO {

class Timer {
  public:
    Timer();
    Timer(const Timer &) = delete;

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
}; // namespace NEO

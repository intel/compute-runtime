/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "gfxEscape.h"

namespace NEO {
class Wddm;

class OSTimeWin : public OSTime {

  public:
    OSTimeWin(OSInterface *osInterface);
    bool getCpuTime(uint64_t *timeStamp) override;
    double getHostTimerResolution() const override;
    uint64_t getCpuRawTimestamp() override;

    static std::unique_ptr<OSTime> create(OSInterface *osInterface);

  protected:
    LARGE_INTEGER frequency;
    OSTimeWin() = default;
    decltype(&QueryPerformanceCounter) QueryPerfomanceCounterFnc = QueryPerformanceCounter;
};

} // namespace NEO

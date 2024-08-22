/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sys_calls_winmm.h"

namespace NEO {

namespace SysCalls {

size_t timeBeginPeriodCalled = 0u;
UINT timeBeginPeriodLastValue = 0u;
size_t timeEndPeriodCalled = 0u;
UINT timeEndPeriodLastValue = 0u;

MMRESULT timeBeginPeriod(UINT period) {
    timeBeginPeriodCalled++;
    timeBeginPeriodLastValue = period;
    return 0u;
}

MMRESULT timeEndPeriod(UINT period) {
    timeEndPeriodCalled++;
    timeEndPeriodLastValue = period;
    return 0u;
}

} // namespace SysCalls
} // namespace NEO
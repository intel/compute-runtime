/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls_winmm.h"
namespace NEO {
namespace SysCalls {
MMRESULT timeBeginPeriod(UINT period) {
    return ::timeBeginPeriod(period);
}

MMRESULT timeEndPeriod(UINT period) {
    return ::timeEndPeriod(period);
}
} // namespace SysCalls
} // namespace NEO
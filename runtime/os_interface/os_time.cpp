/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_time.h"

#include "core/helpers/hw_info.h"

namespace NEO {

double OSTime::getDeviceTimerResolution(HardwareInfo const &hwInfo) {
    return hwInfo.capabilityTable.defaultProfilingTimerResolution;
};
} // namespace NEO

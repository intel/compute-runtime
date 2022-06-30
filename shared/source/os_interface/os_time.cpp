/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"

#include "shared/source/helpers/hw_info.h"

namespace NEO {

double OSTime::getDeviceTimerResolution(HardwareInfo const &hwInfo) {
    return hwInfo.capabilityTable.defaultProfilingTimerResolution;
};
} // namespace NEO

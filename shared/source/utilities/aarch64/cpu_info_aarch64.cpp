/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#include <asm/hwcap.h>

namespace NEO {
void CpuInfo::detect() const {
    features = featureNone;
    uint32_t cpuInfo[4] = {};
    cpuid(cpuInfo, 0u);
    features |= cpuInfo[0] & HWCAP_ASIMD ? featureNeon : featureNone;
    featuresDetected = true;
}
} // namespace NEO

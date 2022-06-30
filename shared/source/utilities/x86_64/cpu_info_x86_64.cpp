/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpu_info.h"

#ifndef BIT
#define BIT(x) (1ull << (x))
#endif

namespace NEO {
void CpuInfo::detect() const {
    uint32_t cpuInfo[4] = {};

    cpuid(cpuInfo, 0u);
    auto numFunctionIds = cpuInfo[0];
    if (numFunctionIds >= 1u) {
        cpuid(cpuInfo, 1u);
        {
            features |= cpuInfo[3] & BIT(19) ? featureClflush : featureNone;
        }
    }

    if (numFunctionIds >= 7u) {
        cpuid(cpuInfo, 7u);
        {
            auto mask = BIT(5) | BIT(3) | BIT(8);
            features |= (cpuInfo[1] & mask) == mask ? featureAvX2 : featureNone;
        }
    }

    cpuid(cpuInfo, 0x80000000);
    auto maxExtendedId = cpuInfo[0];
    if (maxExtendedId >= 0x80000008) {
        cpuid(cpuInfo, 0x80000008);
        {
            virtualAddressSize = (cpuInfo[0] >> 8) & 0xFF;
        }
    }
}
} // namespace NEO

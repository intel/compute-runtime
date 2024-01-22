/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/cpu_info.h"

#ifndef BIT
#define BIT(x) (1ull << (x))
#endif

namespace NEO {
void CpuInfo::detect() const {
    constexpr uint32_t processorInfo = 1;
    constexpr uint32_t extendedFeatures = 0x7;
    constexpr uint32_t extendedFunctionId = 0x80000000;
    constexpr uint32_t addressSize = 0x80000008;
    constexpr size_t eax = 0;
    constexpr size_t ebx = 1;
    constexpr size_t ecx = 2;
    constexpr size_t edx = 3;

    uint32_t cpuInfo[4] = {};

    cpuid(cpuInfo, 0u);
    auto numFunctionIds = cpuInfo[eax];
    if (numFunctionIds >= processorInfo) {
        cpuid(cpuInfo, processorInfo);
        {
            features |= cpuInfo[edx] & BIT(19) ? featureClflush : featureNone;
        }
    }

    if (numFunctionIds >= extendedFeatures) {
        cpuid(cpuInfo, extendedFeatures);
        {
            auto mask = BIT(5) | BIT(3) | BIT(8);
            features |= (cpuInfo[ebx] & mask) == mask ? featureAvX2 : featureNone;

            features |= (cpuInfo[ecx] & BIT(5)) ? featureWaitPkg : featureNone;
        }
    }

    cpuid(cpuInfo, extendedFunctionId);
    auto maxExtendedId = cpuInfo[eax];
    if (maxExtendedId >= addressSize) {
        cpuid(cpuInfo, addressSize);
        {
            virtualAddressSize = (cpuInfo[eax] >> 8) & 0xFF;
        }
    }
    if (debugManager.flags.PrintCpuFlags.get()) {
        printf("CPUFlags:\nCLFlush: %d Avx2: %d WaitPkg: %d\nVirtual Address Size %u\n", !!(features & featureClflush), !!(features & featureAvX2), !!(features & featureWaitPkg), virtualAddressSize);
    }
}
} // namespace NEO

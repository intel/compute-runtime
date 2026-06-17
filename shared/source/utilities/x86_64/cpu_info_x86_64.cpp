/*
 * Copyright (C) 2021-2026 Intel Corporation
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

    features = featureNone;
    uint32_t cpuInfo[4] = {};
    bool osXsave = false;

    cpuid(cpuInfo, 0u);
    auto numFunctionIds = cpuInfo[eax];
    if (numFunctionIds >= processorInfo) {
        cpuid(cpuInfo, processorInfo);
        {
            features |= cpuInfo[edx] & BIT(19) ? featureClflush : featureNone;
            osXsave = !!(cpuInfo[ecx] & BIT(27));
        }
    }

    if (numFunctionIds >= extendedFeatures) {
        cpuid(cpuInfo, extendedFeatures);
        {
            features |= (cpuInfo[ecx] & BIT(5)) ? featureWaitPkg : featureNone;

            if (osXsave) {
                const uint64_t xcr0 = xgetbvFunc(0);

                constexpr uint64_t avx2CpuMask = BIT(5) | BIT(3) | BIT(8);
                constexpr uint64_t avx512CpuMask = BIT(16);

                constexpr uint64_t avx2OsMask = BIT(1) | BIT(2);
                constexpr uint64_t avx512OsMask = BIT(1) | BIT(2) | BIT(5) | BIT(6) | BIT(7);

                if (((cpuInfo[ebx] & avx2CpuMask) == avx2CpuMask) && ((xcr0 & avx2OsMask) == avx2OsMask)) {
                    features |= featureAvX2;
                }

                if (((cpuInfo[ebx] & avx512CpuMask) == avx512CpuMask) && ((xcr0 & avx512OsMask) == avx512OsMask)) {
                    features |= featureAvX512;
                }
            }
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
    featuresDetected = true;
    PRINT_STRING(debugManager.flags.PrintCpuFlags.get(), stdout,
                 "CPUFlags:\nCLFlush: %d Avx2: %d Avx512: %d WaitPkg: %d\nVirtual Address Size %u\n",
                 !!(features & featureClflush), !!(features & featureAvX2), !!(features & featureAvX512), !!(features & featureWaitPkg), virtualAddressSize);
}
} // namespace NEO

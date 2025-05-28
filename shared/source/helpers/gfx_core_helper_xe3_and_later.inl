/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
template <>
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    auto maxThreadsPerEuCount = 1u;
    if (grfCount <= 96u) {
        maxThreadsPerEuCount = 10;
    } else if (grfCount <= 128u) {
        maxThreadsPerEuCount = 8;
    } else if (grfCount <= 160u) {
        maxThreadsPerEuCount = 6;
    } else if (grfCount <= 192u) {
        maxThreadsPerEuCount = 5;
    } else if (grfCount <= 256u) {
        maxThreadsPerEuCount = 4;
    }
    return std::min(hwInfo.gtSystemInfo.ThreadCount, maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount);
}

template <>
uint32_t GfxCoreHelperHw<Family>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    uint32_t numThreadsPerThreadGroup = getThreadsPerWG(simd, totalWorkItems);
    if (debugManager.flags.RemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroup.get() == 1) {
        return numThreadsPerThreadGroup;
    }

    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    const auto &productHelper = rootDeviceEnvironment.getProductHelper();
    const auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto isHeaplessMode = compilerProductHelper.isHeaplessModeEnabled(hwInfo);

    uint32_t maxThreadsPerThreadGroup = 32u;
    if (grfCount == 512) {
        maxThreadsPerThreadGroup = 16u;
    } else if ((grfCount == 256) || (simd == 32u)) {
        // driver limit maxWorkgroupSize to 1024 (NEO-11881) so for simt 32 the max threads per thread group is 32
        maxThreadsPerThreadGroup = 32u;
    } else if (grfCount == 192) {
        maxThreadsPerThreadGroup = 40u;
    } else if (grfCount == 160) {
        maxThreadsPerThreadGroup = 48u;
    } else if (grfCount <= 128) {
        maxThreadsPerThreadGroup = 64u;
    }

    maxThreadsPerThreadGroup = productHelper.adjustMaxThreadsPerThreadGroup(maxThreadsPerThreadGroup, simd, grfCount, isHeaplessMode);

    numThreadsPerThreadGroup = std::min(numThreadsPerThreadGroup, maxThreadsPerThreadGroup);
    DEBUG_BREAK_IF(numThreadsPerThreadGroup * simd > CommonConstants::maxWorkgroupSize);
    return numThreadsPerThreadGroup;
}

} // namespace NEO

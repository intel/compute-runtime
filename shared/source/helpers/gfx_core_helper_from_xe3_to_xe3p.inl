/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t GfxCoreHelperHw<Family>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    uint32_t numThreadsPerThreadGroup = getThreadsPerWG(simd, totalWorkItems);
    if (debugManager.flags.RemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroup.get() == 1) {
        return numThreadsPerThreadGroup;
    }

    const auto &productHelper = rootDeviceEnvironment.getProductHelper();

    uint32_t maxThreadsPerThreadGroup = 64u;
    if (grfCount == 512) {
        maxThreadsPerThreadGroup = 16u;
    } else if ((grfCount == 256) || (simd == 32u)) {
        maxThreadsPerThreadGroup = 32u;
    } else if (grfCount == 192) {
        maxThreadsPerThreadGroup = 40u;
    } else if (grfCount == 160) {
        maxThreadsPerThreadGroup = 48u;
    }

    maxThreadsPerThreadGroup = productHelper.adjustMaxThreadsPerThreadGroup(*rootDeviceEnvironment.getHardwareInfo(), maxThreadsPerThreadGroup, simd, grfCount);

    numThreadsPerThreadGroup = std::min(numThreadsPerThreadGroup, maxThreadsPerThreadGroup);
    DEBUG_BREAK_IF(numThreadsPerThreadGroup * simd > CommonConstants::maxWorkgroupSize);
    return numThreadsPerThreadGroup;
}

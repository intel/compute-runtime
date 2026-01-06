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
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto maxThreadsPerEuCount = 1u;

    const auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();

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

    maxThreadsPerEuCount = releaseHelper->adjustMaxThreadsPerEuCount(maxThreadsPerEuCount, grfCount);

    return std::min(hwInfo.gtSystemInfo.ThreadCount, maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount);
}

} // namespace NEO

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/work_size_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/program/kernel_info.h"

#include <cmath>

namespace NEO {

WorkSizeInfo::WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface, bool disableEUFusion) {
    this->maxWorkGroupSize = maxWorkGroupSize;
    this->hasBarriers = hasBarriers;
    this->simdSize = simdSize;
    this->slmTotalSize = slmTotalSize;
    this->coreFamily = rootDeviceEnvironment.getHardwareInfo()->platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = numThreadsPerSubSlice;
    this->localMemSize = localMemSize;
    this->imgUsed = imgUsed;
    this->yTiledSurfaces = yTiledSurface;

    setMinWorkGroupSize(rootDeviceEnvironment, disableEUFusion);
}

void WorkSizeInfo::setIfUseImg(const KernelInfo &kernelInfo) {
    for (const auto &arg : kernelInfo.kernelDescriptor.payloadMappings.explicitArgs) {
        if (arg.is<ArgDescriptor::argTImage>()) {
            imgUsed = true;
            yTiledSurfaces = true;
            return;
        }
    }
}

void WorkSizeInfo::setMinWorkGroupSize(const RootDeviceEnvironment &rootDeviceEnvironment, bool disableEUFusion) {
    minWorkGroupSize = 0;
    if (hasBarriers) {
        uint32_t maxBarriersPerHSlice = 32;
        minWorkGroupSize = numThreadsPerSubSlice * simdSize / maxBarriersPerHSlice;
    }
    if (slmTotalSize > 0) {
        if (localMemSize < slmTotalSize) {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        }
        UNRECOVERABLE_IF(localMemSize < slmTotalSize);
        minWorkGroupSize = std::max(maxWorkGroupSize / ((localMemSize / slmTotalSize)), minWorkGroupSize);
    }

    const auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (gfxCoreHelper.isFusedEuDispatchEnabled(*rootDeviceEnvironment.getHardwareInfo(), disableEUFusion)) {
        minWorkGroupSize *= 2;
    }
}

void WorkSizeInfo::checkRatio(const size_t workItems[3]) {
    if (slmTotalSize > 0) {
        useRatio = true;
        targetRatio = log((float)workItems[0]) - log((float)workItems[1]);
        useStrictRatio = false;
    } else if (yTiledSurfaces == true) {
        useRatio = true;
        targetRatio = yTilingRatioValue;
        useStrictRatio = true;
    }
}

void WorkSizeInfo::setPreferredWgCountPerSubslice(uint32_t preferredWgCount) {
    preferredWgCountPerSubSlice = preferredWgCount;
}

} // namespace NEO

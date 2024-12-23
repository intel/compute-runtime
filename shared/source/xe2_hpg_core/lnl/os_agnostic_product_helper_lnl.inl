/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"

#include "aubstream/product_family.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::overrideAllocationCacheable(const AllocationData &allocationData) const {
    return allocationData.type == AllocationType::commandBuffer || this->overrideCacheableForDcFlushMitigation(allocationData.type);
}

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    const bool enabled = !isOOQ && queueTaskCount == queueCsr.peekTaskCount() && !queueCsr.directSubmissionRelaxedOrderingEnabled();
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Lnl;
}

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
bool ProductHelperHw<gfxProduct>::blitEnqueuePreferred(bool isWriteToImageFromBuffer) const {
    return isWriteToImageFromBuffer;
}

template <>
bool ProductHelperHw<gfxProduct>::isCachingOnCpuAvailable() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isAdjustDirectSubmissionTimeoutOnThrottleAndAcLineStatusEnabled() const {
    return true;
}

template <>
TimeoutParams ProductHelperHw<gfxProduct>::getDirectSubmissionControllerTimeoutParams(bool acLineConnected, QueueThrottle queueThrottle) const {
    TimeoutParams params{};
    if (acLineConnected) {
        switch (queueThrottle) {
        case NEO::LOW:
            params.maxTimeout = std::chrono::microseconds{500};
            params.timeout = std::chrono::microseconds{500};
            break;
        case NEO::MEDIUM:
            params.maxTimeout = std::chrono::microseconds{4'500};
            params.timeout = std::chrono::microseconds{4'500};
            break;
        case NEO::HIGH:
            params.maxTimeout = std::chrono::microseconds{DirectSubmissionController::defaultTimeout};
            params.timeout = std::chrono::microseconds{DirectSubmissionController::defaultTimeout};
            break;
        default:
            break;
        }
    } else {
        switch (queueThrottle) {
        case NEO::LOW:
            params.maxTimeout = std::chrono::microseconds{500};
            params.timeout = std::chrono::microseconds{500};
            break;
        case NEO::MEDIUM:
            params.maxTimeout = std::chrono::microseconds{2'000};
            params.timeout = std::chrono::microseconds{2'000};
            break;
        case NEO::HIGH:
            params.maxTimeout = std::chrono::microseconds{3'000};
            params.timeout = std::chrono::microseconds{3'000};
            break;
        default:
            break;
        }
    }
    params.timeoutDivisor = 1;
    params.directSubmissionEnabled = true;
    return params;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 16u;
}

} // namespace NEO

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsCmdListHeapSharing() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateComputeModeTracking() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsFrontEndTracking() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsPipelineSelectTracking() const {
    return true;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const {
    uint32_t kernelCount = EventPacketsCount::maxKernelSplit;
    if (L0GfxCoreHelper::usePipeControlMultiKernelEventSync(hwInfo)) {
        kernelCount = 1;
    }
    return kernelCount;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const {
    uint32_t basePackets = getEventMaxKernelCount(hwInfo);
    if (NEO::MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo)) {
        basePackets += L0GfxCoreHelper::useCompactL3FlushEventPacket(hwInfo) ? 0 : 1;
    }

    return basePackets;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsRayTracing() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isZebinAllowed(const NEO::Debugger *debugger) const {
    return true;
}

} // namespace L0

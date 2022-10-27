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
bool L0HwHelperHw<Family>::multiTileCapablePlatform() const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsCmdListHeapSharing(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsStateComputeModeTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsFrontEndTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsPipelineSelectTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
uint32_t L0HwHelperHw<Family>::getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const {
    uint32_t kernelCount = EventPacketsCount::maxKernelSplit;
    if (L0HwHelper::usePipeControlMultiKernelEventSync(hwInfo)) {
        kernelCount = 1;
    }
    return kernelCount;
}

template <typename Family>
uint32_t L0HwHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::HardwareInfo &hwInfo) const {
    uint32_t basePackets = getEventMaxKernelCount(hwInfo);
    if (NEO::MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo)) {
        basePackets += L0HwHelper::useCompactL3FlushEventPacket(hwInfo) ? 0 : 1;
    }

    return basePackets;
}

} // namespace L0

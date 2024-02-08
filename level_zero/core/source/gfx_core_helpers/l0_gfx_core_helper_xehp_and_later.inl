/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

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
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking() const {
    return false;
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
uint32_t L0GfxCoreHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    uint32_t basePackets = getEventMaxKernelCount(hwInfo);
    if (NEO::MemorySynchronizationCommands<Family>::getDcFlushEnable(true, rootDeviceEnvironment)) {
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

template <typename Family>
NEO::HeapAddressModel L0GfxCoreHelperHw<Family>::getPlatformHeapAddressModel() const {
    return NEO::HeapAddressModel::privateHeaps;
}

template <typename Family>
ze_rtas_format_exp_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormat() const {
    return static_cast<ze_rtas_format_exp_t>(RTASDeviceFormatInternal::version1);
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsPrimaryBatchBufferCmdList() const {
    return true;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::platformSupportsImmediateComputeFlushTask() const {
    return true;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getCmdListUpdateCapabilities() const {
    return 1;
}

} // namespace L0

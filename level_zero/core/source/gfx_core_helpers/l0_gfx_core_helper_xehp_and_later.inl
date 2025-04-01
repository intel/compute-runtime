/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"

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
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
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

    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);
    bool flushL3AfterPostSync = productHelper.isL3FlushAfterPostSyncRequired(heaplessEnabled);

    uint32_t basePackets = getEventMaxKernelCount(hwInfo);
    if (NEO::MemorySynchronizationCommands<Family>::getDcFlushEnable(true, rootDeviceEnvironment) && !flushL3AfterPostSync) {
        basePackets += L0GfxCoreHelper::useCompactL3FlushEventPacket(hwInfo, flushL3AfterPostSync) ? 0 : 1;
    }

    return basePackets;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isZebinAllowed(const NEO::Debugger *debugger) const {
    return true;
}

template <typename Family>
NEO::HeapAddressModel L0GfxCoreHelperHw<Family>::getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return NEO::HeapAddressModel::privateHeaps;
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
ze_mutable_command_exp_flags_t L0GfxCoreHelperHw<Family>::getPlatformCmdListUpdateCapabilities() const {
    return ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_ARGUMENTS | ZE_MUTABLE_COMMAND_EXP_FLAG_SIGNAL_EVENT | ZE_MUTABLE_COMMAND_EXP_FLAG_WAIT_EVENTS |
           ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_COUNT | ZE_MUTABLE_COMMAND_EXP_FLAG_GROUP_SIZE | ZE_MUTABLE_COMMAND_EXP_FLAG_GLOBAL_OFFSET | ZE_MUTABLE_COMMAND_EXP_FLAG_KERNEL_INSTRUCTION;
}

} // namespace L0

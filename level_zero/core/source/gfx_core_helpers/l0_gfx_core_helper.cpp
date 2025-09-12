/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

template <>
L0::L0GfxCoreHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0GfxCoreHelper>() const;

namespace L0 {

createL0GfxCoreHelperFunctionType l0GfxCoreHelperFactory[IGFX_MAX_CORE] = {};

std::unique_ptr<L0GfxCoreHelper> L0GfxCoreHelper::create(GFXCORE_FAMILY gfxCore) {
    auto createL0GfxCoreHelperFunc = l0GfxCoreHelperFactory[gfxCore];
    if (createL0GfxCoreHelperFunc == nullptr) {
        return nullptr;
    }
    auto l0GfxCoreHelper = createL0GfxCoreHelperFunc();
    return l0GfxCoreHelper;
}

bool L0GfxCoreHelper::enableFrontEndStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.EnableFrontEndTracking.get() != -1) {
        return !!NEO::debugManager.flags.EnableFrontEndTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsFrontEndTracking();
}

bool L0GfxCoreHelper::enablePipelineSelectStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.EnablePipelineSelectTracking.get() != -1) {
        return !!NEO::debugManager.flags.EnablePipelineSelectTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsPipelineSelectTracking();
}

bool L0GfxCoreHelper::enableStateComputeModeTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.EnableStateComputeModeTracking.get() != -1) {
        return !!NEO::debugManager.flags.EnableStateComputeModeTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsStateComputeModeTracking();
}

bool L0GfxCoreHelper::enableStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.EnableStateBaseAddressTracking.get() != -1) {
        return !!NEO::debugManager.flags.EnableStateBaseAddressTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(rootDeviceEnvironment);
}

bool L0GfxCoreHelper::enableImmediateCmdListHeapSharing(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool cmdlistSupport) {
    if (NEO::debugManager.flags.EnableImmediateCmdListHeapSharing.get() != -1) {
        return !!NEO::debugManager.flags.EnableImmediateCmdListHeapSharing.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    bool platformSupport = l0GfxCoreHelper.platformSupportsCmdListHeapSharing();
    return platformSupport && cmdlistSupport;
}

bool L0GfxCoreHelper::usePipeControlMultiKernelEventSync(const NEO::HardwareInfo &hwInfo) {
    if (NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.get() != -1) {
        return !!NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.get();
    }
    return true;
}

bool L0GfxCoreHelper::useCompactL3FlushEventPacket(const NEO::HardwareInfo &hwInfo, bool flushL3AfterPostSync) {

    if (NEO::debugManager.flags.CompactL3FlushEventPacket.get() != -1) {
        return !!NEO::debugManager.flags.CompactL3FlushEventPacket.get();
    }

    return !flushL3AfterPostSync;
}

bool L0GfxCoreHelper::useDynamicEventPacketsCount(const NEO::HardwareInfo &hwInfo) {
    if (NEO::debugManager.flags.UseDynamicEventPacketsCount.get() != -1) {
        return !!NEO::debugManager.flags.UseDynamicEventPacketsCount.get();
    }
    return true;
}

bool L0GfxCoreHelper::useSignalAllEventPackets(const NEO::HardwareInfo &hwInfo) {
    if (NEO::debugManager.flags.SignalAllEventPackets.get() != -1) {
        return !!NEO::debugManager.flags.SignalAllEventPackets.get();
    }
    return true;
}

NEO::HeapAddressModel L0GfxCoreHelper::getHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.SelectCmdListHeapAddressModel.get() != -1) {
        return static_cast<NEO::HeapAddressModel>(NEO::debugManager.flags.SelectCmdListHeapAddressModel.get());
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.getPlatformHeapAddressModel(rootDeviceEnvironment);
}

bool L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool allowPrimary) {
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    bool value = l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList();
    if (NEO::debugManager.flags.DispatchCmdlistCmdBufferPrimary.get() != -1) {
        value = !!(NEO::debugManager.flags.DispatchCmdlistCmdBufferPrimary.get());
    }
    return value && allowPrimary;
}

bool L0GfxCoreHelper::useImmediateComputeFlushTask(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.UseImmediateFlushTask.get() != -1) {
        return !!(NEO::debugManager.flags.UseImmediateFlushTask.get());
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask();
}

ze_mutable_command_exp_flags_t L0GfxCoreHelper::getCmdListUpdateCapabilities(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.OverrideCmdListUpdateCapability.get() != -1) {
        return static_cast<ze_mutable_command_exp_flags_t>(NEO::debugManager.flags.OverrideCmdListUpdateCapability.get());
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities();
}

ze_record_replay_graph_exp_flags_t L0GfxCoreHelper::getRecordReplayGraphCapabilities(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::debugManager.flags.OverrideRecordReplayGraphCapability.get() != -1) {
        return static_cast<ze_record_replay_graph_exp_flags_t>(NEO::debugManager.flags.OverrideRecordReplayGraphCapability.get());
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities();
}

} // namespace L0

template <>
L0::L0GfxCoreHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0GfxCoreHelper>() const {
    return *static_cast<L0::L0GfxCoreHelper *>(apiGfxCoreHelper.get());
}

void NEO::RootDeviceEnvironment::initApiGfxCoreHelper() {
    apiGfxCoreHelper = L0::L0GfxCoreHelper::create(this->getHardwareInfo()->platform.eRenderCoreFamily);
}

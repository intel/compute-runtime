/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_gfx_core_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

template <>
L0::L0GfxCoreHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0GfxCoreHelper>() const;

namespace L0 {

createL0GfxCoreHelperFunctionType l0GfxCoreHelperFactory[IGFX_MAX_CORE] = {};

std::unique_ptr<L0GfxCoreHelper> L0GfxCoreHelper::create(GFXCORE_FAMILY gfxCore) {
    auto createL0GfxCoreHelperFunc = l0GfxCoreHelperFactory[gfxCore];
    auto l0GfxCoreHelper = createL0GfxCoreHelperFunc();
    return l0GfxCoreHelper;
}

bool L0GfxCoreHelper::enableFrontEndStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnableFrontEndTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableFrontEndTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsFrontEndTracking();
}

bool L0GfxCoreHelper::enablePipelineSelectStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnablePipelineSelectTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnablePipelineSelectTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsPipelineSelectTracking();
}

bool L0GfxCoreHelper::enableStateComputeModeTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnableStateComputeModeTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableStateComputeModeTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsStateComputeModeTracking();
}

bool L0GfxCoreHelper::enableStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnableStateBaseAddressTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableStateBaseAddressTracking.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    return l0GfxCoreHelper.platformSupportsStateBaseAddressTracking();
}

bool L0GfxCoreHelper::enableImmediateCmdListHeapSharing(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool cmdlistSupport) {
    if (NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get() != -1) {
        return !!NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get();
    }
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    bool platformSupport = l0GfxCoreHelper.platformSupportsCmdListHeapSharing();
    return platformSupport && cmdlistSupport;
}

bool L0GfxCoreHelper::usePipeControlMultiKernelEventSync(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.get() != -1) {
        return !!NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.get();
    }
    return true;
}

bool L0GfxCoreHelper::useCompactL3FlushEventPacket(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.CompactL3FlushEventPacket.get() != -1) {
        return !!NEO::DebugManager.flags.CompactL3FlushEventPacket.get();
    }
    return true;
}

bool L0GfxCoreHelper::useDynamicEventPacketsCount(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.UseDynamicEventPacketsCount.get() != -1) {
        return !!NEO::DebugManager.flags.UseDynamicEventPacketsCount.get();
    }
    return true;
}

bool L0GfxCoreHelper::useSignalAllEventPackets(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.SignalAllEventPackets.get() != -1) {
        return !!NEO::DebugManager.flags.SignalAllEventPackets.get();
    }
    return true;
}

} // namespace L0

template <>
L0::L0GfxCoreHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0GfxCoreHelper>() const {
    return *static_cast<L0::L0GfxCoreHelper *>(apiGfxCoreHelper.get());
}

void NEO::RootDeviceEnvironment::initApiGfxCoreHelper() {
    apiGfxCoreHelper = L0::L0GfxCoreHelper::create(this->getHardwareInfo()->platform.eRenderCoreFamily);
}

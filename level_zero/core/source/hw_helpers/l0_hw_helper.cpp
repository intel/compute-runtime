/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

template <>
L0::L0HwHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0HwHelper>() const;

namespace L0 {

L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE] = {};

L0HwHelper &L0HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *l0HwHelperFactory[gfxCore];
}

bool L0HwHelper::enableFrontEndStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnableFrontEndTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableFrontEndTracking.get();
    }
    auto &l0CoreHelper = rootDeviceEnvironment.getHelper<L0CoreHelper>();
    return l0CoreHelper.platformSupportsFrontEndTracking();
}

bool L0HwHelper::enablePipelineSelectStateTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnablePipelineSelectTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnablePipelineSelectTracking.get();
    }
    auto &l0CoreHelper = rootDeviceEnvironment.getHelper<L0CoreHelper>();
    return l0CoreHelper.platformSupportsPipelineSelectTracking();
}

bool L0HwHelper::enableStateComputeModeTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (NEO::DebugManager.flags.EnableStateComputeModeTracking.get() != -1) {
        return !!NEO::DebugManager.flags.EnableStateComputeModeTracking.get();
    }
    auto &l0CoreHelper = rootDeviceEnvironment.getHelper<L0CoreHelper>();
    return l0CoreHelper.platformSupportsStateComputeModeTracking();
}

bool L0HwHelper::enableImmediateCmdListHeapSharing(const NEO::RootDeviceEnvironment &rootDeviceEnvironment, bool cmdlistSupport) {
    if (NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get() != -1) {
        return !!NEO::DebugManager.flags.EnableImmediateCmdListHeapSharing.get();
    }
    auto &l0CoreHelper = rootDeviceEnvironment.getHelper<L0CoreHelper>();
    bool platformSupport = l0CoreHelper.platformSupportsCmdListHeapSharing();
    return platformSupport && cmdlistSupport;
}

bool L0HwHelper::usePipeControlMultiKernelEventSync(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.get() != -1) {
        return !!NEO::DebugManager.flags.UsePipeControlMultiKernelEventSync.get();
    }
    return true;
}

bool L0HwHelper::useCompactL3FlushEventPacket(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.CompactL3FlushEventPacket.get() != -1) {
        return !!NEO::DebugManager.flags.CompactL3FlushEventPacket.get();
    }
    return true;
}

bool L0HwHelper::useDynamicEventPacketsCount(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.UseDynamicEventPacketsCount.get() != -1) {
        return !!NEO::DebugManager.flags.UseDynamicEventPacketsCount.get();
    }
    return true;
}

bool L0HwHelper::useSignalAllEventPackets(const NEO::HardwareInfo &hwInfo) {
    if (NEO::DebugManager.flags.SignalAllEventPackets.get() != -1) {
        return !!NEO::DebugManager.flags.SignalAllEventPackets.get();
    }
    return true;
}

} // namespace L0

template <>
L0::L0HwHelper &NEO::RootDeviceEnvironment::getHelper<L0::L0HwHelper>() const {
    auto &apiHelper = L0::L0HwHelper::get(this->getHardwareInfo()->platform.eRenderCoreFamily);
    return apiHelper;
}

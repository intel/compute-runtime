/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_hw_helper_base.inl"
#include "opencl/source/helpers/cl_hw_helper_xehp_and_later.inl"

namespace NEO {

using Family = XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;

template <>
void populateFactoryTable<ClHwHelperHw<Family>>() {
    extern ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE];
    clHwHelperFactory[gfxCore] = &ClHwHelperHw<Family>::get();
}

template <>
bool ClHwHelperHw<Family>::requiresNonAuxMode(const ArgDescPointer &argAsPtr, const HardwareInfo &hwInfo) const {
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->allowStatelessCompression(hwInfo)) {
        return false;
    } else {
        return !argAsPtr.isPureStateful();
    }
}

template <>
bool ClHwHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo, const HardwareInfo &hwInfo) const {
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->allowStatelessCompression(hwInfo)) {
        return false;
    } else {
        return hasStatelessAccessToBuffer(kernelInfo);
    }
}

template <>
inline bool ClHwHelperHw<Family>::allowCompressionForContext(const ClDevice &clDevice, const Context &context) const {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    auto &hwInfo = clDevice.getHardwareInfo();
    if (context.containsMultipleSubDevices(rootDeviceIndex) && HwHelperHw<Family>::get().isWorkaroundRequired(REVISION_A0, REVISION_A1, hwInfo)) {
        return false;
    }
    return true;
}

template <>
bool ClHwHelperHw<Family>::isSupportedKernelThreadArbitrationPolicy() const { return false; }

template <>
std::vector<uint32_t> ClHwHelperHw<Family>::getSupportedThreadArbitrationPolicies() const {
    return {};
}

template <>
cl_version ClHwHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(12, 5, makeDeviceRevision(hwInfo));
}

template class ClHwHelperHw<Family>;

} // namespace NEO

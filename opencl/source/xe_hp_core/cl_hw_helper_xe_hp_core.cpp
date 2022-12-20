/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
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
void populateFactoryTable<ClGfxCoreHelperHw<Family>>() {
    extern ClGfxCoreHelper *clGfxCoreHelperFactory[IGFX_MAX_CORE];
    clGfxCoreHelperFactory[gfxCore] = &ClGfxCoreHelperHw<Family>::get();
}

template <>
bool ClGfxCoreHelperHw<Family>::requiresNonAuxMode(const ArgDescPointer &argAsPtr, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();

    if (productHelper.allowStatelessCompression(hwInfo)) {
        return false;
    } else {
        return !argAsPtr.isPureStateful();
    }
}

template <>
bool ClGfxCoreHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();

    if (productHelper.allowStatelessCompression(hwInfo)) {
        return false;
    } else {
        return hasStatelessAccessToBuffer(kernelInfo);
    }
}

template <>
inline bool ClGfxCoreHelperHw<Family>::allowCompressionForContext(const ClDevice &clDevice, const Context &context) const {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    auto &hwInfo = clDevice.getHardwareInfo();
    auto &productHelper = clDevice.getDevice().getProductHelper();
    if (context.containsMultipleSubDevices(rootDeviceIndex) && GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hwInfo, productHelper)) {
        return false;
    }
    return true;
}

template <>
bool ClGfxCoreHelperHw<Family>::isSupportedKernelThreadArbitrationPolicy() const { return false; }

template <>
std::vector<uint32_t> ClGfxCoreHelperHw<Family>::getSupportedThreadArbitrationPolicies() const {
    return {};
}

template <>
cl_version ClGfxCoreHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(12, 5, makeDeviceRevision(hwInfo));
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

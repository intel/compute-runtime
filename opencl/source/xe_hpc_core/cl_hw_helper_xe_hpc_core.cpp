/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/helpers/cl_hw_helper_base.inl"
#include "opencl/source/helpers/cl_hw_helper_xehp_and_later.inl"

namespace NEO {

using Family = XeHpcCoreFamily;
static auto gfxCore = IGFX_XE_HPC_CORE;

#include "opencl/source/helpers/cl_hw_helper_pvc_and_later.inl"

template <>
void populateFactoryTable<ClHwHelperHw<Family>>() {
    extern ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE];
    clHwHelperFactory[gfxCore] = &ClHwHelperHw<Family>::get();
}

template <>
bool ClHwHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return false;
}

template <>
cl_version ClHwHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(12, 8, makeDeviceRevision(hwInfo));
}

template class ClHwHelperHw<Family>;

} // namespace NEO

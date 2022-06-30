/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_hw_helper_base.inl"
#include "opencl/source/helpers/cl_hw_helper_bdw_and_later.inl"

namespace NEO {

using Family = TGLLPFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void populateFactoryTable<ClHwHelperHw<Family>>() {
    extern ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE];
    clHwHelperFactory[gfxCore] = &ClHwHelperHw<Family>::get();
}

template <>
cl_device_feature_capabilities_intel ClHwHelperHw<Family>::getSupportedDeviceFeatureCapabilities(const HardwareInfo &hwInfo) const {
    return CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
}

template <>
cl_version ClHwHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(12, 0, makeDeviceRevision(hwInfo));
}

template class ClHwHelperHw<Family>;

} // namespace NEO

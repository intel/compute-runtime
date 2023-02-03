/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen12LpFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"

template <>
cl_device_feature_capabilities_intel ClGfxCoreHelperHw<Family>::getSupportedDeviceFeatureCapabilities(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
}

template <>
cl_version ClGfxCoreHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(12, 0, makeDeviceRevision(hwInfo));
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

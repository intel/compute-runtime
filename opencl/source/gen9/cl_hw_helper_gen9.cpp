/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_hw_helper_base.inl"
#include "opencl/source/helpers/cl_hw_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen9Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
void populateFactoryTable<ClGfxCoreHelperHw<Family>>() {
    extern ClGfxCoreHelper *clGfxCoreHelperFactory[IGFX_MAX_CORE];
    clGfxCoreHelperFactory[gfxCore] = &ClGfxCoreHelperHw<Family>::get();
}

template <>
cl_version ClGfxCoreHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(9, 0, 0);
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

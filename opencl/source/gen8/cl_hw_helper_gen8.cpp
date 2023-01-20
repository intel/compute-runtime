/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_hw_helper_base.inl"
#include "opencl/source/helpers/cl_hw_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen8Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "opencl/source/helpers/cl_hw_helper_factory_init.inl"

template <>
cl_version ClGfxCoreHelperHw<Family>::getDeviceIpVersion(const HardwareInfo &hwInfo) const {
    return makeDeviceIpVersion(8, 0, 0);
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

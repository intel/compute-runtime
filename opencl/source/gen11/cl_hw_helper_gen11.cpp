/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_hw_helper_base.inl"

#include "hw_cmds.h"

namespace NEO {

using Family = ICLFamily;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
void populateFactoryTable<ClHwHelperHw<Family>>() {
    extern ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE];
    clHwHelperFactory[gfxCore] = &ClHwHelperHw<Family>::get();
}

template class ClHwHelperHw<Family>;

} // namespace NEO

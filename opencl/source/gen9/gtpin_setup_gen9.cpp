/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/kernel/implicit_args.h"

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_bdw_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef Gen9Family Family;
static const auto gfxFamily = IGFX_GEN9_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_9;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableGen9 {
    GTPinEnableGen9() {
        gtpinGfxCoreHelperFactory[gfxFamily] = &GTPinGfxCoreHelperHw<Family>::get();
    }
};

static GTPinEnableGen9 gtpinEnable;

} // namespace NEO

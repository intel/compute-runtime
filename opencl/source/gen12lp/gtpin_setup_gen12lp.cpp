/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/gen12lp/hw_cmds_base.h"

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_bdw_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef Gen12LpFamily Family;
static const auto gfxFamily = IGFX_GEN12LP_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_12_1;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableGen12LP {
    GTPinEnableGen12LP() {
        gtpinGfxCoreHelperFactory[gfxFamily] = &GTPinGfxCoreHelperHw<Family>::get();
    }
};

static GTPinEnableGen12LP gtpinEnable;

} // namespace NEO

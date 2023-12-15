/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/kernel/implicit_args_helper.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.inl"
#include "opencl/source/gtpin/gtpin_gfx_core_helper_bdw_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef Gen8Family Family;
static const auto gfxFamily = IGFX_GEN8_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() const {
    return gtpin::GTPIN_GEN_8;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableGen8 {
    GTPinEnableGen8() {
        gtpinGfxCoreHelperFactory[gfxFamily] = GTPinGfxCoreHelperHw<Family>::create;
    }
};

static GTPinEnableGen8 gtpinEnable;

} // namespace NEO

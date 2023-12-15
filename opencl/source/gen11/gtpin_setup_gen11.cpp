/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/kernel/implicit_args_helper.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.inl"
#include "opencl/source/gtpin/gtpin_gfx_core_helper_bdw_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef Gen11Family Family;
static const auto gfxFamily = IGFX_GEN11_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() const {
    return gtpin::GTPIN_GEN_11;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableGen11 {
    GTPinEnableGen11() {
        gtpinGfxCoreHelperFactory[gfxFamily] = GTPinGfxCoreHelperHw<Family>::create;
    }
};

static GTPinEnableGen11 gtpinEnable;

} // namespace NEO

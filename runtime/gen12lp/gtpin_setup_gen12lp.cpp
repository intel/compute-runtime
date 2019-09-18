/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_hw_helper.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef TGLLPFamily Family;
static const auto gfxFamily = IGFX_GEN12LP_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_INVALID;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableGen12LP {
    GTPinEnableGen12LP() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableGen12LP gtpinEnable;

} // namespace NEO

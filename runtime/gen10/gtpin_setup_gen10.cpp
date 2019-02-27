/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_hw_helper.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace OCLRT {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef CNLFamily Family;
static const auto gfxFamily = IGFX_GEN10_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_10;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableGen10 {
    GTPinEnableGen10() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableGen10 gtpinEnable;

} // namespace OCLRT

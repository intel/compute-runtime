/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

#include <map>

namespace NEO {

using Family = Gen11Family;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
void ImageHw<Family>::appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex, bool useGlobalAtomics) {
    if (hasAlphaChannel(&imageFormat)) {
        surfaceState->setSampleTapDiscardDisable(RENDER_SURFACE_STATE::SAMPLE_TAP_DISCARD_DISABLE_ENABLE);
    }
}

} // namespace NEO

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

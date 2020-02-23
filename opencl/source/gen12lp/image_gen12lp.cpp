/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/gen12lp/helpers_gen12lp.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/image.inl"

namespace NEO {

typedef TGLLPFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <typename GfxFamily>
void ImageHw<GfxFamily>::setMediaSurfaceRotation(void *memory) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setRotation(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE);
    surfaceState->setXOffset(0);
    surfaceState->setYOffset(0);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setSurfaceMemoryObjectControlStateIndexToMocsTables(value);
}

// clang-format off
#include "opencl/source/mem_obj/image_tgllp_plus.inl"
#include "opencl/source/mem_obj/image_factory_init.inl"
// clang-format on
} // namespace NEO

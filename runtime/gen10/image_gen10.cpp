/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/image.inl"
#include <map>

namespace OCLRT {

typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

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

#include "runtime/mem_obj/image_factory_init.inl"
} // namespace OCLRT

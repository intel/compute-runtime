/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen8/hw_cmds.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/image.inl"

#include <map>

namespace NEO {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

template <>
void ImageHw<Family>::setMediaSurfaceRotation(void *) {}

template <>
void ImageHw<Family>::setSurfaceMemoryObjectControlStateIndexToMocsTable(void *, uint32_t) {}

template <>
void ImageHw<Family>::setMipTailStartLod(RENDER_SURFACE_STATE *surfaceState) {}

#include "runtime/mem_obj/image_factory_init.inl"
} // namespace NEO

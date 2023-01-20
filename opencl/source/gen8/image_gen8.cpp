/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

#include <map>

namespace NEO {

using Family = Gen8Family;
static auto gfxCore = IGFX_GEN8_CORE;

template <>
void ImageHw<Family>::setMediaSurfaceRotation(void *) {}

template <>
void ImageHw<Family>::setSurfaceMemoryObjectControlState(void *, uint32_t) {}

} // namespace NEO

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

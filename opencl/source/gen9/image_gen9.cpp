/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = Gen9Family;
static auto gfxCore = IGFX_GEN9_CORE;
} // namespace NEO

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = Gen12LpFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;
} // namespace NEO
#include "opencl/source/mem_obj/image_tgllp_and_later.inl"

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

typedef Gen12LpFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

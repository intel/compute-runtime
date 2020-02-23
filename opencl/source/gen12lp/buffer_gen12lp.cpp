/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef TGLLPFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

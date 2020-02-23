/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "opencl/source/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

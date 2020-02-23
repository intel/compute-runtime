/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "opencl/source/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

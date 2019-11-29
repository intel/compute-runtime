/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "runtime/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef TGLLPFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

#include "runtime/mem_obj/buffer_factory_init.inl"
} // namespace NEO

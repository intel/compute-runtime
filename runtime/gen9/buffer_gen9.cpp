/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen9/hw_cmds.h"
#include "runtime/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

#include "runtime/mem_obj/buffer_factory_init.inl"
} // namespace NEO

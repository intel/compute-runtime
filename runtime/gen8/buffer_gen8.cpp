/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/mem_obj/buffer.inl"

namespace OCLRT {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "runtime/mem_obj/buffer_factory_init.inl"
} // namespace OCLRT

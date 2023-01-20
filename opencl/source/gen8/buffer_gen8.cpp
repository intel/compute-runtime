/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

typedef Gen8Family Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

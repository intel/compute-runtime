/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "opencl/source/mem_obj/buffer_bdw_plus.inl"

namespace NEO {

typedef ICLFamily Family;
static auto gfxCore = IGFX_GEN11_CORE;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

typedef XeHpFamily Family;
static auto gfxCore = IGFX_XE_HP_CORE;

template class BufferHw<Family>;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

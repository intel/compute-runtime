/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

using Family = Xe2HpgCoreFamily;
static auto gfxCore = IGFX_XE2_HPG_CORE;

template class BufferHw<Family>;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

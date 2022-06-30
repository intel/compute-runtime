/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

typedef XE_HPG_COREFamily Family;
static auto gfxCore = IGFX_XE_HPG_CORE;

template class BufferHw<Family>;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

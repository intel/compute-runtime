/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

using Family = Xe3CoreFamily;
static auto gfxCore = IGFX_XE3_CORE;

template class BufferHw<Family>;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

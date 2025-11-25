/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer_base.inl"

#include "hw_cmds_xe3p_core.h"

namespace NEO {

using Family = Xe3pCoreFamily;
static auto gfxCore = NEO::xe3pCoreEnumValue;

template class BufferHw<Family>;

#include "opencl/source/mem_obj/buffer_factory_init.inl"
} // namespace NEO

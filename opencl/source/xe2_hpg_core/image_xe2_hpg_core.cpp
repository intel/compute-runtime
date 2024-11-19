/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = Xe2HpgCoreFamily;
static auto gfxCore = IGFX_XE2_HPG_CORE;

} // namespace NEO
#include "opencl/source/mem_obj/image_tgllp_and_later.inl"
#include "opencl/source/mem_obj/image_xe2_and_later.inl"

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

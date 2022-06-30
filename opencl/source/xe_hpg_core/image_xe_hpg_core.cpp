/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_base.h"

#include "opencl/source/mem_obj/image.inl"

namespace NEO {

using Family = XE_HPG_COREFamily;
static auto gfxCore = IGFX_XE_HPG_CORE;
} // namespace NEO
#include "opencl/source/mem_obj/image_tgllp_and_later.inl"

// factory initializer
#include "opencl/source/mem_obj/image_factory_init.inl"

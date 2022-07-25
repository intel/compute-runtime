/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
using Family = NEO::XeHpgCoreFamily;
constexpr static auto gfxCore = IGFX_XE_HPG_CORE;

#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"

namespace NEO {
#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

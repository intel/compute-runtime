/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"
using Family = NEO::XeHpFamily;
constexpr static auto gfxCore = IGFX_XE_HP_CORE;

#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"

namespace NEO {
#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"

namespace NEO {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

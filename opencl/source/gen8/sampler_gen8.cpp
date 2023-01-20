/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"

namespace NEO {

typedef Gen8Family Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

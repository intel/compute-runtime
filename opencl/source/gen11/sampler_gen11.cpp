/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"

namespace NEO {

typedef Gen11Family Family;
static auto gfxCore = IGFX_GEN11_CORE;

#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

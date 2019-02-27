/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sampler/sampler.h"
#include "runtime/sampler/sampler.inl"

#include "hw_cmds.h"

namespace OCLRT {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "runtime/sampler/sampler_factory_init.inl"
} // namespace OCLRT

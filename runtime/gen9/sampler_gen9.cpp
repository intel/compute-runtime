/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen9/hw_cmds.h"
#include "runtime/sampler/sampler.h"
#include "runtime/sampler/sampler.inl"

namespace NEO {

typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

#include "runtime/sampler/sampler_factory_init.inl"
} // namespace NEO

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "runtime/sampler/sampler.h"
#include "runtime/sampler/sampler.inl"

namespace NEO {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

#include "runtime/sampler/sampler_factory_init.inl"
} // namespace NEO

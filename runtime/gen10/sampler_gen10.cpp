/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/sampler/sampler.h"
#include "runtime/sampler/sampler.inl"

namespace OCLRT {

typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

#include "runtime/sampler/sampler_factory_init.inl"
} // namespace OCLRT

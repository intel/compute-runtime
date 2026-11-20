/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
using Family = NEO::XeHpcCoreFamily;
constexpr static auto gfxCore = IGFX_XE_HPC_CORE;
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/sampler/sampler.h"

namespace NEO {
template <>
void SamplerHw<Family>::setArg(void *memory, const RootDeviceEnvironment &rootDeviceEnvironment) {}

#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO

/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_bdw_and_later.inl"

namespace NEO {

using Family = Gen11Family;
static auto gfxCore = IGFX_GEN11_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_xehp_and_later.inl"

namespace NEO {

using Family = XeHpcCoreFamily;
static auto gfxCore = IGFX_XE_HPC_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_pvc_and_later.inl"

template <>
bool ClGfxCoreHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return false;
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

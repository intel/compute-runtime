/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_xehp_and_later.inl"

namespace NEO {

using Family = Xe3CoreFamily;
static auto gfxCore = IGFX_XE3_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_pvc_and_later.inl"

template <>
bool ClGfxCoreHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return false;
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

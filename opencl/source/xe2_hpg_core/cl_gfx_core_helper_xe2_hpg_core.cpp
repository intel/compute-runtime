/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "opencl/source/helpers/cl_gfx_core_helper_base.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_xehp_and_later.inl"

namespace NEO {

using Family = Xe2HpgCoreFamily;
static auto gfxCore = IGFX_XE2_HPG_CORE;

#include "opencl/source/helpers/cl_gfx_core_helper_factory_init.inl"
#include "opencl/source/helpers/cl_gfx_core_helper_pvc_and_later.inl"

template <>
bool ClGfxCoreHelperHw<Family>::requiresAuxResolves(const KernelInfo &kernelInfo) const {
    return false;
}

template <>
bool ClGfxCoreHelperHw<Family>::isLimitationForPreemptionNeeded() const {
    // mid thread preemption will disable with protected mode
    return true;
}

template class ClGfxCoreHelperHw<Family>;

} // namespace NEO

/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

constexpr static auto gfxProduct = IGFX_PVC;

#include "shared/source/xe_hpc_core/os_agnostic_product_helper_xe_hpc_core.inl"
#include "shared/source/xe_hpc_core/pvc/os_agnostic_product_helper_pvc.inl"

namespace NEO {

template class ProductHelperHw<gfxProduct>;

} // namespace NEO

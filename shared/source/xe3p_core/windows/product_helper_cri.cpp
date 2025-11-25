/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_cri.h"

constexpr static auto gfxProduct = NEO::criProductEnumValue;

// keep files below
#include "shared/source/os_interface/windows/product_helper_xe2_and_later_wddm.inl"
#include "shared/source/xe3p_core/cri/os_agnostic_product_helper_cri.inl"
#include "shared/source/xe3p_core/os_agnostic_product_helper_xe3p_core.inl"

template class NEO::ProductHelperHw<gfxProduct>;

/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/gen12lp/hw_info_adls.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_ALDERLAKE_S;

#include "shared/source/gen12lp/adls/os_agnostic_product_helper_adls.inl"
#include "shared/source/gen12lp/os_agnostic_product_helper_gen12lp.inl"

template class NEO::ProductHelperHw<gfxProduct>;

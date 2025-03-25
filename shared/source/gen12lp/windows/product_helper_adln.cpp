/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"
#include "shared/source/gen12lp/hw_info_adln.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_ALDERLAKE_N;

#include "shared/source/gen12lp/adln/os_agnostic_product_helper_adln.inl"
#include "shared/source/gen12lp/os_agnostic_product_helper_gen12lp.inl"

template class NEO::ProductHelperHw<gfxProduct>;

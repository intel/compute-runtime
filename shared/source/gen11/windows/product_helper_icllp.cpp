/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_icllp.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_bdw_and_later.inl"

constexpr static auto gfxProduct = IGFX_ICELAKE_LP;

#include "shared/source/gen11/icllp/os_agnostic_product_helper_icllp.inl"
#include "shared/source/os_interface/product_helper_before_gen12lp.inl"

template class NEO::ProductHelperHw<gfxProduct>;

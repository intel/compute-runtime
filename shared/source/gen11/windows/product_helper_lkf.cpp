/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_bdw_and_later.inl"

constexpr static auto gfxProduct = IGFX_LAKEFIELD;

#include "shared/source/os_interface/product_helper_before_gen12lp.inl"

template class NEO::ProductHelperHw<gfxProduct>;

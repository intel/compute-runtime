/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_bdw.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_bdw_and_later.inl"

constexpr static auto gfxProduct = IGFX_BROADWELL;

#include "shared/source/gen8/bdw/os_agnostic_product_helper_bdw.inl"
#include "shared/source/os_interface/product_helper_before_gen12lp.inl"

template class NEO::ProductHelperHw<gfxProduct>;

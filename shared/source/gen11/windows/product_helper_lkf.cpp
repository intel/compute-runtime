/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/product_helper_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_LAKEFIELD;

#include "shared/source/gen11/lkf/os_agnostic_product_helper_lkf.inl"

template class NEO::ProductHelperHw<gfxProduct>;

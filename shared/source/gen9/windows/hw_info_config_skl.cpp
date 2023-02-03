/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_skl.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_SKYLAKE;

#include "shared/source/gen9/skl/os_agnostic_product_helper_skl.inl"

template class NEO::ProductHelperHw<gfxProduct>;

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ALDERLAKE_S;

#include "shared/source/gen12lp/os_agnostic_hw_info_config_adls.inl"
#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO

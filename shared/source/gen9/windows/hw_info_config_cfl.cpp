/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_COFFEELAKE;

#include "shared/source/gen9/cfl/os_agnostic_hw_info_config_cfl.inl"

template class HwInfoConfigHw<gfxProduct>;

} // namespace NEO

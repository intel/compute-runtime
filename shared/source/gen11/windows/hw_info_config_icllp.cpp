/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_ICELAKE_LP;

#include "shared/source/gen11/os_agnostic_hw_info_config_icllp.inl"

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO

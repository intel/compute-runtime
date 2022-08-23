/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_kbl.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

constexpr static auto gfxProduct = IGFX_KABYLAKE;
#include "shared/source/gen9/kbl/os_agnostic_hw_info_config_kbl.inl"

namespace NEO {
template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO

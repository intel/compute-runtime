/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
namespace NEO {

constexpr static auto gfxProduct = IGFX_ALDERLAKE_P;
#include "shared/test/common/helpers/mock_hw_info_config_hw.inl"
template struct MockHwInfoConfigHw<gfxProduct>;
} // namespace NEO
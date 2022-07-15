/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
bool CompilerHwInfoConfigHw<gfxProduct>::isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt;
}

} // namespace NEO

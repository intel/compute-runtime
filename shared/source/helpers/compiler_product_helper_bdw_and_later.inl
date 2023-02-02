/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt;
}

} // namespace NEO

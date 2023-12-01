/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_PVC;

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, const LinuxSysmanImp *pLinuxSysmanImp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, const LinuxSysmanImp *pLinuxSysmanImp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <>
void SysmanProductHelperHw<gfxProduct>::getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) {
    if (performanceFactor > halfOfMaxPerformanceFactor) {
        *pMultiplier = 1;
    } else {
        *pMultiplier = 0.5;
    }
}

} // namespace Sysman
} // namespace L0

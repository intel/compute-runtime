/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_DG1;

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, const LinuxSysmanImp *pLinuxSysmanImp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0

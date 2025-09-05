/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
ze_rtas_format_exp_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExp() const {
    return static_cast<ze_rtas_format_exp_t>(RTASDeviceFormatInternal::version1);
}

template <typename Family>
ze_rtas_format_ext_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExt() const {
    return static_cast<ze_rtas_format_ext_t>(RTASDeviceFormatInternal::version1);
}

} // namespace L0

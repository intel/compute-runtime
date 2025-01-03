/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
ze_rtas_format_exp_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormat() const {
    return static_cast<ze_rtas_format_exp_t>(RTASDeviceFormatInternal::version2);
}

template <typename Family>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU;
}
} // namespace L0

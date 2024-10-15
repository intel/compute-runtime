/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getGrfRegisterCount(uint32_t *regPtr) const {
    bool largeGrfModeEnabled = false;
    largeGrfModeEnabled = regPtr[0] & 0x2000;
    if (largeGrfModeEnabled) {
        return 256;
    }
    return 128;
}

} // namespace L0

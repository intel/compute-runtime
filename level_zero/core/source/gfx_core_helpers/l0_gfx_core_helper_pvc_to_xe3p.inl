/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
size_t L0GfxCoreHelperHw<Family>::getMaxFillPatternSizeForCopyEngine() const {
    return sizeof(uint8_t);
}

} // namespace L0

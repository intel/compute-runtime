/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {
template <typename Family>
bool L0GfxCoreHelperHw<Family>::isResumeWARequired() {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::synchronizedDispatchSupported() const {
    return true;
}

template <typename Family>
size_t L0GfxCoreHelperHw<Family>::getMaxFillPatternSizeForCopyEngine() const {
    return sizeof(uint8_t);
}

} // namespace L0

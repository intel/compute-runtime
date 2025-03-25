/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForQueues(bool heaplessEnabled) const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getInternalHeapsPreallocated() const {
    if (debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get() != -1) {
        return debugManager.flags.SetAmountOfInternalHeapsToPreallocate.get();
    }
    return 1u;
}

} // namespace NEO

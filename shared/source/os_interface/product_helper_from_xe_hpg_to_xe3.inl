/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForQueues() const {
    return true;
}

} // namespace NEO

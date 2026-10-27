/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::getStorageInfoLocalOnlyFlag(LocalMemAllocationMode usmDeviceAllocationMode, bool defaultValue) const {
    return getLocalOnlyFlagForMode(usmDeviceAllocationMode, defaultValue);
}
} // namespace NEO

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
    switch (usmDeviceAllocationMode) {
    case LocalMemAllocationMode::hwDefault:
        return defaultValue;
    case LocalMemAllocationMode::localOnly:
        return true;
    case LocalMemAllocationMode::localPreferred:
        return false;
    default:
        UNRECOVERABLE_IF(true);
    }
}
} // namespace NEO

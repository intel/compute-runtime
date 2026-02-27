/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <>
bool CompilerProductHelperHw<gfxProduct>::isHeaplessModeEnabled(const HardwareInfo &hwInfo) const {
    if (!hwInfo.featureTable.flags.ftrHeaplessMode) {
        return false;
    }
    if (debugManager.flags.Enable64BitAddressing.get() != -1) {
        return (debugManager.flags.Enable64BitAddressing.get() == 1);
    }
    return true;
}

template <>
bool CompilerProductHelperHw<gfxProduct>::failBuildProgramWithBufferStatefulAccessPreference() const {
    return true;
}

} // namespace NEO

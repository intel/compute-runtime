/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isInterruptSupported(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    return compilerProductHelper.isHeaplessModeEnabled(*rootDeviceEnvironment.getHardwareInfo());
}
} // namespace NEO

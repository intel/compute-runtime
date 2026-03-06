/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isInterruptSupported(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto wddm = (rootDeviceEnvironment.osInterface != nullptr && rootDeviceEnvironment.osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::wddm)
                    ? rootDeviceEnvironment.osInterface->getDriverModel()->as<Wddm>()
                    : nullptr;
    if (wddm == nullptr) {
        return false;
    }
    return wddm->isNativeFenceAvailable() && compilerProductHelper.isHeaplessModeEnabled(hwInfo);
}
} // namespace NEO
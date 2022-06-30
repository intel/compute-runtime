/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <sstream>

namespace NEO {

bool comparePciIdBusNumberWDDM(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2) {
    // BDF sample format is : 00:02.0
    auto bdfDevice1 = rootDeviceEnvironment1->osInterface->getDriverModel()->as<NEO::Wddm>()->getAdapterBDF();

    auto bdfDevice2 = rootDeviceEnvironment2->osInterface->getDriverModel()->as<NEO::Wddm>()->getAdapterBDF();

    if (bdfDevice1.Bus != bdfDevice2.Bus) {
        return (bdfDevice1.Bus < bdfDevice2.Bus);
    }

    if (bdfDevice1.Device != bdfDevice2.Device) {
        return (bdfDevice1.Device < bdfDevice2.Device);
    }

    return bdfDevice1.Function < bdfDevice2.Function;
}

void ExecutionEnvironment::sortNeoDevicesWDDM() {
    const auto pciOrderVar = DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.get();
    if (pciOrderVar) {
        std::sort(rootDeviceEnvironments.begin(), rootDeviceEnvironments.end(), comparePciIdBusNumberWDDM);
    }
}

} // namespace NEO

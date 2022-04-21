/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include <sstream>

namespace NEO {

bool comparePciIdBusNumber(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2) {
    // BDF sample format is : 00:02.0
    rootDeviceEnvironment1.get()->osInterface->getDriverModel()->as<NEO::Drm>()->queryAdapterBDF();
    auto bdfDevice1 = rootDeviceEnvironment1.get()->osInterface->getDriverModel()->as<NEO::Drm>()->getAdapterBDF();
    auto domain1 = rootDeviceEnvironment1.get()->osInterface->getDriverModel()->as<NEO::Drm>()->getPciDomain();

    rootDeviceEnvironment2.get()->osInterface->getDriverModel()->as<NEO::Drm>()->queryAdapterBDF();
    auto bdfDevice2 = rootDeviceEnvironment2.get()->osInterface->getDriverModel()->as<NEO::Drm>()->getAdapterBDF();
    auto domain2 = rootDeviceEnvironment2.get()->osInterface->getDriverModel()->as<NEO::Drm>()->getPciDomain();

    if (domain1 != domain2) {
        return (domain1 < domain2);
    }

    if (bdfDevice1.Bus != bdfDevice2.Bus) {
        return (bdfDevice1.Bus < bdfDevice2.Bus);
    }

    if (bdfDevice1.Device != bdfDevice2.Device) {
        return (bdfDevice1.Device < bdfDevice2.Device);
    }

    return bdfDevice1.Function < bdfDevice2.Function;
}

void ExecutionEnvironment::sortNeoDevices() {
    const auto pciOrderVar = DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.get();
    if (pciOrderVar) {

        std::vector<uint32_t> presort_index;
        for (uint32_t i = 0; i < rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            presort_index.push_back(drm->getRootDeviceIndex());
        }

        std::sort(rootDeviceEnvironments.begin(), rootDeviceEnvironments.end(), comparePciIdBusNumber);

        for (uint32_t i = 0; i < rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            if (drm->getRootDeviceIndex() != presort_index[i]) {
                drm->setRootDeviceIndex(presort_index[i]);
            }
        }
    }
}

} // namespace NEO

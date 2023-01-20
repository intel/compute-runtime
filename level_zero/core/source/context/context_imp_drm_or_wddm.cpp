/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"

namespace L0 {

bool ContextImp::isShareableMemory(const void *exportDesc, bool exportableMemory, NEO::Device *neoDevice) {
    if (exportableMemory) {
        return true;
    }
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        NEO::DriverModelType driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (!exportDesc && driverType == NEO::DriverModelType::WDDM) {
            return true;
        }
    }

    return false;
}

void *ContextImp::getMemHandlePtr(ze_device_handle_t hDevice,
                                  uint64_t handle,
                                  NEO::AllocationType allocationType,
                                  ze_ipc_memory_flags_t flags) {
    L0::Device *device = L0::Device::fromHandle(hDevice);
    auto neoDevice = device->getNEODevice();
    NEO::DriverModelType driverType = NEO::DriverModelType::UNKNOWN;
    if (neoDevice->getRootDeviceEnvironment().osInterface) {
        driverType = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
    }
    bool isNTHandle = this->getDriverHandle()->getMemoryManager()->isNTHandle(NEO::toOsHandle(reinterpret_cast<void *>(handle)), device->getNEODevice()->getRootDeviceIndex());
    if (isNTHandle) {
        return this->driverHandle->importNTHandle(hDevice,
                                                  reinterpret_cast<void *>(handle),
                                                  allocationType);
    } else if (driverType == NEO::DriverModelType::DRM) {
        return this->driverHandle->importFdHandle(Device::fromHandle(hDevice)->getNEODevice(),
                                                  flags,
                                                  handle,
                                                  allocationType,
                                                  nullptr);
    } else {
        return nullptr;
    }
}

} // namespace L0

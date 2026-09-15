/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_device.h"

#include <va/va_backend.h>

namespace NEO {
namespace LEO {
ClDevice *VADevice::getRootDeviceFromVaDisplay(Platform *pPlatform, VADisplay vaDisplay) {
    VADisplayContextP pDisplayContextTest = reinterpret_cast<VADisplayContextP>(vaDisplay);
    UNRECOVERABLE_IF(pDisplayContextTest->vadpy_magic != 0x56414430);
    VADriverContextP pDriverContextTest = pDisplayContextTest->pDriverContext;
    int deviceFd = *static_cast<int *>(pDriverContextTest->drm_state);

    UNRECOVERABLE_IF(deviceFd < 0);

    auto devicePath = NEO::getPciPath(deviceFd);

    if (devicePath == std::nullopt) {
        return nullptr;
    }

    for (size_t i = 0; i < pPlatform->getDevices().size(); ++i) {
        auto device = pPlatform->getDevices()[i].get();
        NEO::Device *neoDevice = &device->getDevice();

        auto *drm = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>();
        auto pciPath = drm->getPciPath();
        if (devicePath == pciPath) {
            return device;
        }
    }
    return nullptr;
}
} // namespace LEO
} // namespace NEO

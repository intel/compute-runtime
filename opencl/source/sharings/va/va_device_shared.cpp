/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/va/va_device.h"

#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <va/va_backend.h>

namespace NEO {
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

    for (size_t i = 0; i < pPlatform->getNumDevices(); ++i) {
        auto device = pPlatform->getClDevice(i);
        NEO::Device *neoDevice = &device->getDevice();

        auto *drm = neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>();
        auto pciPath = drm->getPciPath();
        if (devicePath == pciPath) {
            return device;
        }
    }
    return nullptr;
}
} // namespace NEO

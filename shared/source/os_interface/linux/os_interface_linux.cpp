/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_interface_linux.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;
bool OSInterface::newResourceImplicitFlush = true;
bool OSInterface::gpuIdleImplicitFlush = true;
bool OSInterface::requiresSupportForWddmTrimNotification = false;

bool OSInterface::isDebugAttachAvailable() const {
    if (driverModel) {
        return driverModel->as<Drm>()->isDebugAttachAvailable();
    }
    return false;
}

bool initDrmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex,
                        RootDeviceEnvironment *rootDeviceEnv,
                        std::unique_ptr<OSInterface> &dstOsInterface, std::unique_ptr<MemoryOperationsHandler> &dstMemoryOpsHandler) {
    auto hwDeviceIdDrm = std::unique_ptr<HwDeviceIdDrm>(reinterpret_cast<HwDeviceIdDrm *>(hwDeviceId.release()));

    Drm *drm = Drm::create(std::move(hwDeviceIdDrm), *rootDeviceEnv);
    if (!drm) {
        return false;
    }

    dstOsInterface.reset(new OSInterface());
    dstOsInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    auto hardwareInfo = rootDeviceEnv->getMutableHardwareInfo();
    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    if (hwConfig->configureHwInfoDrm(hardwareInfo, hardwareInfo, dstOsInterface.get())) {
        return false;
    }
    dstMemoryOpsHandler = DrmMemoryOperationsHandler::create(*drm, rootDeviceIndex);

    [[maybe_unused]] bool result = rootDeviceEnv->initAilConfiguration();
    DEBUG_BREAK_IF(!result);

    return true;
}

} // namespace NEO

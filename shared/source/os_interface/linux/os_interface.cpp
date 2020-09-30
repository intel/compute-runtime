/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_interface.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;
bool OSInterface::newResourceImplicitFlush = true;
bool OSInterface::gpuIdleImplicitFlush = true;

OSInterface::OSInterfaceImpl::OSInterfaceImpl() = default;
OSInterface::OSInterfaceImpl::~OSInterfaceImpl() = default;
void OSInterface::OSInterfaceImpl::setDrm(Drm *drm) {
    this->drm.reset(drm);
}

OSInterface::OSInterface() {
    osInterfaceImpl = new OSInterfaceImpl();
}

OSInterface::~OSInterface() {
    delete osInterfaceImpl;
}

bool OSInterface::are64kbPagesEnabled() {
    return osEnabled64kbPages;
}

uint32_t OSInterface::getDeviceHandle() const {
    return 0;
}

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) {
    Drm *drm = Drm::create(std::move(hwDeviceId), *this);
    if (!drm) {
        return false;
    }

    osInterface.reset(new OSInterface());
    osInterface->get()->setDrm(drm);
    auto hardwareInfo = getMutableHardwareInfo();
    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    if (hwConfig->configureHwInfo(hardwareInfo, hardwareInfo, osInterface.get())) {
        return false;
    }
    memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, rootDeviceIndex);
    return true;
}
} // namespace NEO

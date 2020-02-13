/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/os_interface.h"

#include "core/execution_environment/execution_environment.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/hw_info_config.h"
#include "core/os_interface/linux/drm_memory_operations_handler.h"
#include "core/os_interface/linux/drm_neo.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;

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

void OSInterface::setGmmInputArgs(void *args) {}

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId) {
    Drm *drm = Drm::create(std::move(hwDeviceId), *this);
    if (!drm) {
        return false;
    }

    memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
    osInterface.reset(new OSInterface());
    osInterface->get()->setDrm(drm);
    auto hardwareInfo = executionEnvironment.getMutableHardwareInfo();
    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    if (hwConfig->configureHwInfo(hardwareInfo, hardwareInfo, osInterface.get())) {
        return false;
    }
    return true;
}
} // namespace NEO

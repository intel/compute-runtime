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

void OSInterface::setGmmInputArgs(void *args) {
    reinterpret_cast<GMM_INIT_IN_ARGS *>(args)->FileDescriptor = this->get()->getDrm()->getFileDescriptor();
}

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId) {
    Drm *drm = Drm::create(std::move(hwDeviceId), *this);
    if (!drm) {
        return false;
    }

    memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandler>();
    osInterface.reset(new OSInterface());
    osInterface->get()->setDrm(drm);
    auto hardwareInfo = getMutableHardwareInfo();
    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->platform.eProductFamily);
    if (hwConfig->configureHwInfo(hardwareInfo, hardwareInfo, osInterface.get())) {
        return false;
    }
    return true;
}
} // namespace NEO

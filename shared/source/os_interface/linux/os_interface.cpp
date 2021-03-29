/*
 * Copyright (C) 2017-2021 Intel Corporation
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
#include "shared/source/os_interface/linux/sys_calls.h"

#include <optional>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;
bool OSInterface::newResourceImplicitFlush = true;
bool OSInterface::gpuIdleImplicitFlush = true;

OSInterface::OSInterfaceImpl::OSInterfaceImpl() = default;
OSInterface::OSInterfaceImpl::~OSInterfaceImpl() = default;
void OSInterface::OSInterfaceImpl::setDrm(Drm *drm) {
    this->drm.reset(drm);
}

bool OSInterface::OSInterfaceImpl::isDebugAttachAvailable() const {
    if (drm) {
        return drm->isDebugAttachAvailable();
    }
    return false;
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

bool OSInterface::isDebugAttachAvailable() const {
    return osInterfaceImpl->isDebugAttachAvailable();
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

std::optional<std::string> OSInterface::OSInterfaceImpl::getPciPath(int deviceFd) {
    char path[256] = {0};
    size_t pathlen = 256;

    if (SysCalls::getDevicePath(deviceFd, path, pathlen)) {
        return std::nullopt;
    }

    if (SysCalls::access(path, F_OK)) {
        return std::nullopt;
    }

    int readLinkSize = 0;
    char devicePath[256] = {0};
    readLinkSize = SysCalls::readlink(path, devicePath, pathlen);

    if (readLinkSize == -1) {
        return std::nullopt;
    }

    std::string_view devicePathView(devicePath, static_cast<size_t>(readLinkSize));
    devicePathView = devicePathView.substr(devicePathView.find("/drm/render") - 7u, 7u);

    return std::string(devicePathView);
}
} // namespace NEO

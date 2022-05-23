/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
class MockDeviceTimeDrm : public DeviceTimeDrm {
  public:
    using DeviceTimeDrm::pDrm;
    MockDeviceTimeDrm() : DeviceTimeDrm(nullptr) {
    }
};

class MockOSTimeLinux : public OSTimeLinux {
  public:
    MockOSTimeLinux(OSInterface *osInterface)
        : OSTimeLinux(osInterface, std::make_unique<MockDeviceTimeDrm>()) {
    }
    void setResolutionFunc(resolutionFunc_t func) {
        this->resolutionFunc = func;
    }
    void setGetTimeFunc(getTimeFunc_t func) {
        this->getTimeFunc = func;
    }
    void updateDrm(Drm *drm) {
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        static_cast<MockDeviceTimeDrm *>(this->deviceTime.get())->pDrm = drm;
        static_cast<MockDeviceTimeDrm *>(this->deviceTime.get())->timestampTypeDetect();
    }
    static std::unique_ptr<MockOSTimeLinux> create(OSInterface *osInterface) {
        return std::unique_ptr<MockOSTimeLinux>(new MockOSTimeLinux(osInterface));
    }

    MockDeviceTimeDrm *getDeviceTime() {
        return static_cast<MockDeviceTimeDrm *>(this->deviceTime.get());
    }
};
} // namespace NEO

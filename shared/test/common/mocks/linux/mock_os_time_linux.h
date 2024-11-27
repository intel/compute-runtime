/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
class MockDeviceTimeDrm : public DeviceTimeDrm {
  public:
    using DeviceTimeDrm::DeviceTimeDrm;
    using DeviceTimeDrm::pDrm;

    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override {
        getGpuCpuTimeImplCalled++;
        if (callBaseGetGpuCpuTimeImpl) {
            return DeviceTimeDrm::getGpuCpuTimeImpl(pGpuCpuTime, osTime);
        }
        *pGpuCpuTime = gpuCpuTimeValue;
        return getGpuCpuTimeImplResult;
    }

    double getDynamicDeviceTimerResolution() const override {
        if (callGetDynamicDeviceTimerResolution) {
            return DeviceTimeDrm::getDynamicDeviceTimerResolution();
        }
        return dynamicDeviceTimerResolutionValue;
    }

    bool callBaseGetGpuCpuTimeImpl = true;
    TimeQueryStatus getGpuCpuTimeImplResult = TimeQueryStatus::success;
    TimeStampData gpuCpuTimeValue{};
    uint32_t getGpuCpuTimeImplCalled = 0;

    bool callGetDynamicDeviceTimerResolution = false;
    double dynamicDeviceTimerResolutionValue = 1.0;
};

class MockOSTimeLinux : public OSTimeLinux {
  public:
    using OSTimeLinux::maxGpuTimeStamp;
    MockOSTimeLinux(OSInterface &osInterface)
        : OSTimeLinux(osInterface, std::make_unique<MockDeviceTimeDrm>(osInterface)) {
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
    }
    static std::unique_ptr<MockOSTimeLinux> create(OSInterface &osInterface) {
        return std::unique_ptr<MockOSTimeLinux>(new MockOSTimeLinux(osInterface));
    }

    MockDeviceTimeDrm *getDeviceTime() {
        return static_cast<MockDeviceTimeDrm *>(this->deviceTime.get());
    }
};
} // namespace NEO

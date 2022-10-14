/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"

#include <cstdint>
#include <functional>

namespace NEO {

class MockDriverModel : public NEO::DriverModel {
  public:
    MockDriverModel() : NEO::DriverModel(NEO::DriverModelType::UNKNOWN) {}

    void setGmmInputArgs(void *args) override {}

    uint32_t getDeviceHandle() const override { return {}; }

    NEO::PhysicalDevicePciBusInfo getPciBusInfo() const override { return pciBusInfo; }

    size_t getMaxMemAllocSize() const override {
        return maxAllocSize;
    }

    bool isGpuHangDetected(NEO::OsContext &osContext) override {
        if (isGpuHangDetectedSideEffect) {
            std::invoke(isGpuHangDetectedSideEffect);
        }

        return isGpuHangDetectedToReturn;
    }

    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override { return pciSpeedInfo; }

    PhysicalDevicePciSpeedInfo pciSpeedInfo{};
    PhysicalDevicePciBusInfo pciBusInfo{};
    bool isGpuHangDetectedToReturn{};
    std::function<void()> isGpuHangDetectedSideEffect{};
    size_t maxAllocSize = 0;
};

class MockDriverModelWDDM : public MockDriverModel {
  public:
    MockDriverModelWDDM() : MockDriverModel() {
        driverModelType = DriverModelType::WDDM;
    }
};

class MockDriverModelDRM : public MockDriverModel {
  public:
    MockDriverModelDRM() : MockDriverModel() {
        driverModelType = DriverModelType::DRM;
    }
};
} // namespace NEO

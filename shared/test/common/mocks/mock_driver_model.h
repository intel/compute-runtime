/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include <cstdint>
#include <functional>

namespace NEO {

class MockDriverModel : public NEO::DriverModel {
  public:
    MockDriverModel() : MockDriverModel(NEO::DriverModelType::unknown) {}
    MockDriverModel(DriverModelType driverModelType) : DriverModel(driverModelType) {}

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

    const HardwareInfo *getHardwareInfo() const override { return nullptr; }

    PhysicalDevicePciSpeedInfo pciSpeedInfo{};
    PhysicalDevicePciBusInfo pciBusInfo{};
    bool isGpuHangDetectedToReturn{};
    std::function<void()> isGpuHangDetectedSideEffect{};
    size_t maxAllocSize = 0;
};

class MockDriverModelWDDM : public MockDriverModel {
  public:
    MockDriverModelWDDM() : MockDriverModel() {
        driverModelType = DriverModelType::wddm;
    }
};

class MockDriverModelDRM : public MockDriverModel {
  public:
    MockDriverModelDRM() : MockDriverModel() {
        driverModelType = DriverModelType::drm;
    }
};
} // namespace NEO

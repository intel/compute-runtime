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

class MockDriverModel : public NEO::DriverModel {
  public:
    MockDriverModel() : NEO::DriverModel(NEO::DriverModelType::UNKNOWN) {}

    void setGmmInputArgs(void *args) override {}

    uint32_t getDeviceHandle() const override { return {}; }

    NEO::PhysicalDevicePciBusInfo getPciBusInfo() const override { return pciBusInfo; }

    size_t getMaxMemAllocSize() const override {
        return 0;
    }

    bool isGpuHangDetected(NEO::OsContext &osContext) override {
        if (isGpuHangDetectedSideEffect) {
            std::invoke(isGpuHangDetectedSideEffect);
        }

        return isGpuHangDetectedToReturn;
    }

    PhyicalDevicePciSpeedInfo getPciSpeedInfo() const override { return {}; }

    NEO::PhysicalDevicePciBusInfo pciBusInfo{};
    bool isGpuHangDetectedToReturn{};
    std::function<void()> isGpuHangDetectedSideEffect{};
};
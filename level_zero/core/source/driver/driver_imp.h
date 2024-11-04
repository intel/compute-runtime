/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver.h"

#include <atomic>
#include <mutex>
#include <string>

namespace L0 {

class DriverImp : public Driver {
  public:
    ze_result_t driverInit() override;

    void initialize(ze_result_t *result) override;
    unsigned int getPid() const override {
        return pid;
    }
    void tryInitGtpin() override;
    ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDrivers) override;

  protected:
    uint32_t pid = 0;
    std::once_flag initDriverOnce;
    static ze_result_t initStatus;
    std::atomic<bool> gtPinInitializationNeeded{false};
    std::mutex gtpinInitMtx;
};

struct L0EnvVariables {
    std::string affinityMask;
    uint32_t programDebugging;
    bool metrics;
    bool pin;
    bool sysman;
    bool pciIdDeviceOrder;
    bool fp64Emulation;
    std::string deviceHierarchyMode;
};

} // namespace L0

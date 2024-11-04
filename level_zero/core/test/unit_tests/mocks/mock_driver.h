/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <atomic>

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverImp> : public ::L0::DriverImp {
    using ::L0::DriverImp::gtPinInitializationNeeded;
    using ::L0::DriverImp::pid;
};

using Driver = WhiteBox<::L0::DriverImp>;

template <>
struct Mock<Driver> : public Driver {
    Mock();
    ~Mock() override;

    ze_result_t driverInit() override {
        initCalledCount++;
        if (initCalledCount == 1) {
            pid = NEO::SysCalls::getCurrentProcessId();
        }
        if (driverInitCallBase) {
            return DriverImp::driverInit();
        }
        if (failInitDriver) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDriverHandles) override {
        if (driverGetCallBase) {
            return DriverImp::driverHandleGet(pCount, phDriverHandles);
        }
        if (*pCount == 0) {
            *pCount = 1;
            return ZE_RESULT_SUCCESS;
        }
        if (phDriverHandles == nullptr) {
            return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
        }
        *phDriverHandles = reinterpret_cast<ze_driver_handle_t>(&mockDriverhandle);
        return ZE_RESULT_SUCCESS;
    }

    void initialize(ze_result_t *result) override {
        initializeCalledCount++;
        if (initializeCallBase) {
            DriverImp::initialize(result);
            return;
        }
        pid = NEO::SysCalls::getCurrentProcessId();

        if (failInitDriver) {
            *result = ZE_RESULT_ERROR_UNINITIALIZED;
        }
        *result = ZE_RESULT_SUCCESS;
    }

    Driver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
    uint32_t initializeCalledCount = 0;
    bool failInitDriver = false;
    bool driverInitCallBase = false;
    bool driverGetCallBase = true;
    bool initializeCallBase = false;
    uint64_t mockDriverhandle = 0xffff;
};

} // namespace ult
} // namespace L0

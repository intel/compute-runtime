/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/device_factory.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver_handle.h"
#include "level_zero/core/source/driver_imp.h"

#include <memory>
#include <thread>

namespace L0 {

std::unique_ptr<_ze_driver_handle_t> GlobalDriver;
uint32_t driverCount = 1;

void DriverImp::initialize(bool *result) {
    *result = false;

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    UNRECOVERABLE_IF(nullptr == executionEnvironment);

    executionEnvironment->incRefInternal();
    auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->decRefInternal();
    if (!devices.empty()) {
        GlobalDriver.reset(DriverHandle::create(std::move(devices)));
        if (GlobalDriver != nullptr) {
            *result = true;
        }
    }
}

bool DriverImp::initStatus(false);

ze_result_t DriverImp::driverInit(ze_init_flag_t flag) {
    std::call_once(initDriverOnce, [this]() {
        bool result;
        this->initialize(&result);
        initStatus = result;
    });
    return ((initStatus) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNINITIALIZED);
}

ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDriverHandles) {
    if (*pCount == 0) {
        *pCount = driverCount;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > driverCount) {
        *pCount = driverCount;
    }

    if (phDriverHandles == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phDriverHandles[i] = GlobalDriver.get();
    }

    return ZE_RESULT_SUCCESS;
}

static DriverImp driverImp;
Driver *Driver::driver = &driverImp;

ze_result_t init(ze_init_flag_t flag) { return Driver::get()->driverInit(flag); }

} // namespace L0

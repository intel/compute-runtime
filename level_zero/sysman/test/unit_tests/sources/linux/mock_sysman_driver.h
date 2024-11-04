/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/driver/sysman_driver_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockSysmanDriver : public ::L0::Sysman::SysmanDriverImp {
    MockSysmanDriver() {
        previousDriver = driver;
        driver = this;
    }
    ~MockSysmanDriver() override {
        driver = previousDriver;
    }

    ze_result_t driverInit() override {
        initCalledCount++;
        if (useBaseDriverInit) {
            return SysmanDriverImp::driverInit();
        }
        return ZE_RESULT_SUCCESS;
    }

    void initialize(ze_result_t *result) override {
        if (useBaseInit) {
            SysmanDriverImp::initialize(result);
            return;
        }
        if (sysmanInitFail) {
            *result = ZE_RESULT_ERROR_UNINITIALIZED;
            return;
        }
        *result = ZE_RESULT_SUCCESS;
        return;
    }

    ::L0::Sysman::SysmanDriver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
    bool useBaseDriverInit = false;
    bool sysmanInitFail = false;
    bool useBaseInit = true;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2023 Intel Corporation
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

    ze_result_t driverInit(ze_init_flags_t flag) override {
        initCalledCount++;
        return ZE_RESULT_SUCCESS;
    }

    ::L0::Sysman::SysmanDriver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
};

} // namespace ult
} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <atomic>

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverImp> : public ::L0::DriverImp {
};

using Driver = WhiteBox<::L0::DriverImp>;

template <>
struct Mock<Driver> : public Driver {
    Mock();
    ~Mock() override;

    ze_result_t driverInit(ze_init_flags_t flag) override {
        initCalledCount++;
        if (failInitDriver) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        return ZE_RESULT_SUCCESS;
    }

    Driver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
    bool failInitDriver = false;
};

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
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

    MOCK_METHOD(ze_result_t,
                driverInit,
                (ze_init_flag_t),
                (override));
    MOCK_METHOD(void,
                initialize,
                (bool *result),
                (override));

    ze_result_t mockInit(ze_init_flag_t) { return this->DriverImp::driverInit(ZE_INIT_FLAG_NONE); }
    void mockInitialize(bool *result) { *result = true; }

    Driver *previousDriver = nullptr;
};

} // namespace ult
} // namespace L0

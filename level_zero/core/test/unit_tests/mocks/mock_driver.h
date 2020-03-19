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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::DriverImp> : public ::L0::DriverImp {
};

using Driver = WhiteBox<::L0::DriverImp>;

template <>
struct Mock<Driver> : public DriverImp {
    Mock();
    virtual ~Mock();

    MOCK_METHOD1(driverInit, ze_result_t(ze_init_flag_t));
    MOCK_METHOD1(initialize, void(bool *result));

    ze_result_t mockInit(ze_init_flag_t) { return this->DriverImp::driverInit(ZE_INIT_FLAG_NONE); }
    void mockInitialize(bool *result) { *result = true; }

    Driver *previousDriver = nullptr;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

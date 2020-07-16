/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Context> : public ::L0::Context {};

using Context = WhiteBox<::L0::Context>;

template <>
struct Mock<Context> : public Context {
    Mock();
    ~Mock() override;

    MOCK_METHOD(ze_result_t,
                destroy,
                (),
                (override));
    MOCK_METHOD(ze_result_t,
                getStatus,
                (),
                (override));
    MOCK_METHOD(DriverHandle *,
                getDriverHandle,
                (),
                (override));
};

} // namespace ult
} // namespace L0

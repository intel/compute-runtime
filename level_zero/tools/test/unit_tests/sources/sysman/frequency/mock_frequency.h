/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/frequency/frequency_imp.h"
#include "sysman/frequency/os_frequency.h"
#include "sysman/sysman.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsFrequency> : public OsFrequency {

    Mock<OsFrequency>() = default;

    MOCK_METHOD1(getMin, ze_result_t(double &min));
    MOCK_METHOD1(setMin, ze_result_t(double min));
    MOCK_METHOD1(getMax, ze_result_t(double &max));
    MOCK_METHOD1(setMax, ze_result_t(double max));
    MOCK_METHOD1(getRequest, ze_result_t(double &request));
    MOCK_METHOD1(getTdp, ze_result_t(double &tdp));
    MOCK_METHOD1(getActual, ze_result_t(double &actual));
    MOCK_METHOD1(getEfficient, ze_result_t(double &efficient));
    MOCK_METHOD1(getMaxVal, ze_result_t(double &maxVal));
    MOCK_METHOD1(getMinVal, ze_result_t(double &minVal));
    MOCK_METHOD1(getThrottleReasons, ze_result_t(uint32_t &throttleReasons));
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

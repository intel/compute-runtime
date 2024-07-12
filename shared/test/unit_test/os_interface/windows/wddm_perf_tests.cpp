/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/wddm_fixture.h"

namespace NEO {

using WddmPerfTests = WddmFixture;

TEST_F(WddmPerfTests, givenCorrectArgumentsWhenPerfOpenEuStallStreamIsCalledThenReturnFailure) {
    constexpr uint32_t samplingGranularity = 251u;
    constexpr uint32_t gpuClockPeriodNs = 1000000000ull;
    constexpr uint32_t samplingUnit = 1;
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_FALSE(wddm->perfOpenEuStallStream(notifyEveryNReports, samplingPeriodNs));
}

TEST_F(WddmPerfTests, givenCorrectArgumentsWhenPerfReadEuStallStreamIsCalledThenReturnFailure) {
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_FALSE(wddm->perfReadEuStallStream(&pRawData, &pRawDataSize));
}

TEST_F(WddmPerfTests, givenCorrectArgumentsWhenPerfDisableEuStallStreamIsCalledThenReturnFailure) {
    EXPECT_FALSE(wddm->perfDisableEuStallStream());
}

} // namespace NEO

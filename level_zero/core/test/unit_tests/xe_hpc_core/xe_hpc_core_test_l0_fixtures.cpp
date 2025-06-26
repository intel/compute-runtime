/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/xe_hpc_core/xe_hpc_core_test_l0_fixtures.h"

namespace NEO {
struct HardwareInfo;
} // namespace NEO

using namespace L0;
using namespace ult;

void DeviceFixtureXeHpcTests::checkIfCallingGetMemoryPropertiesWithNonNullPtrThenMaxClockRateReturnZero(HardwareInfo *hwInfo) {
    uint32_t count = 0;
    ze_result_t res = device->getMemoryProperties(&count, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    ze_device_memory_properties_t memProperties = {};
    res = device->getMemoryProperties(&count, &memProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, count);

    EXPECT_EQ(memProperties.maxClockRate, 0u);
}
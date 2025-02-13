/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/metrics/metric_query_pool_fixture.h"

#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/core/source/driver/driver_imp.h"

namespace L0 {
namespace ult {
void MetricQueryPoolTest::SetUp() {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    MetricContextFixture::setUp();
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    driverHandle.reset(DriverHandle::create(NEO::DeviceFactory::createDevices(*executionEnvironment), L0EnvVariables{}, &returnValue));
}

void MetricQueryPoolTest::TearDown() {
    MetricContextFixture::tearDown();
    driverHandle.reset();
}

void MultiDeviceMetricQueryPoolTest::SetUp() {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    MetricMultiDeviceFixture::setUp();
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    driverHandle.reset(DriverHandle::create(NEO::DeviceFactory::createDevices(*executionEnvironment), L0EnvVariables{}, &returnValue));
}

void MultiDeviceMetricQueryPoolTest::TearDown() {
    MetricMultiDeviceFixture::tearDown();
    driverHandle.reset();
}

} // namespace ult
} // namespace L0

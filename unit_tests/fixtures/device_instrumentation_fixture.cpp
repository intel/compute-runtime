/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_instrumentation_fixture.h"

#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/mocks/mock_device.h"

namespace NEO {
void DeviceInstrumentationFixture::SetUp(bool instrumentation) {
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo);
    hwInfo->capabilityTable.instrumentationEnabled = instrumentation;
    device = std::unique_ptr<Device>(Device::create<RootDevice>(executionEnvironment, 0));
}
} // namespace NEO

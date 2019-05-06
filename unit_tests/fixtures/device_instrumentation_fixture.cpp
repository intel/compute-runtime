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
    if (instrumentation)
        hwInfo->capabilityTable.instrumentationEnabled = true;
    device = std::unique_ptr<Device>(Device::create<Device>(executionEnvironment, 0));
}
} // namespace NEO

/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_instrumentation_fixture.h"

#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {
void DeviceInstrumentationFixture::setUp(bool instrumentation) {
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    hwInfo->capabilityTable.instrumentationEnabled = instrumentation;
    device = std::make_unique<ClDevice>(*Device::create<RootDevice>(executionEnvironment, 0), platform());
}

} // namespace NEO

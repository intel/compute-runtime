/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/preemption_fixture.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"
#include "patch_g7.h"

using namespace NEO;

DevicePreemptionTests::DevicePreemptionTests() = default;
DevicePreemptionTests::~DevicePreemptionTests() = default;

void DevicePreemptionTests::SetUp() {
    if (dbgRestore == nullptr) {
        dbgRestore.reset(new DebugManagerStateRestore());
    }

    device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    executionEnvironment.reset(new iOpenCL::SPatchExecutionEnvironment);
    memset(executionEnvironment.get(), 0, sizeof(iOpenCL::SPatchExecutionEnvironment));

    ASSERT_NE(nullptr, device);

    waTable = &device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;
}

void DevicePreemptionTests::TearDown() {
    dbgRestore.reset();
}

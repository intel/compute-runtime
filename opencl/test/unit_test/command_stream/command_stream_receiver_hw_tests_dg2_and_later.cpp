/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

using namespace NEO;
using MatcherIsRTCapable = IsAtLeastXeCore;

struct CommandStreamReceiverHwTestDg2AndLater : public DeviceFixture,
                                                public HardwareParse,
                                                public ::testing::Test {

    void SetUp() override {
        DeviceFixture::setUp();
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
        DeviceFixture::tearDown();
    }
};

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HPG_COREWhenCheckingNewResourceImplicitFlushThenReturnFalse, IsNotXeHpgCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsNewResourceImplicitFlush());
}

HWTEST2_F(CommandStreamReceiverHwTestDg2AndLater, givenNotXE_HP_COREWhenCheckingNewResourceGpuIdleThenReturnFalse, IsAtLeastXeCore) {
    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto osContext = pDevice->getDefaultEngine().osContext;
    commandStreamReceiver.setupContext(*osContext);

    EXPECT_FALSE(commandStreamReceiver.checkPlatformSupportsGpuIdleImplicitFlush());
}

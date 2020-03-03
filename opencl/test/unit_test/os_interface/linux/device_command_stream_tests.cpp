/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "opencl/source/command_stream/aub_command_stream_receiver.h"
#include "opencl/source/os_interface/linux/device_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/mock_aub_center_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"
#include "test.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct DeviceCommandStreamLeaksTest : ::testing::Test {
    void SetUp() override {
        HardwareInfo *hwInfo = nullptr;
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);
    }

    ExecutionEnvironment *executionEnvironment;
};

HWTEST_F(DeviceCommandStreamLeaksTest, Create) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(false, *executionEnvironment, 0));
    DrmMockSuccess mockDrm;
    EXPECT_NE(nullptr, ptr);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(false, *executionEnvironment, 0));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_EQ(drmCsr->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWithAubDumWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(true, *executionEnvironment, 0));
    auto drmCsrWithAubDump = (CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *)ptr.get();
    EXPECT_EQ(drmCsrWithAubDump->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
    auto aubCSR = static_cast<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *>(ptr.get())->aubCSR.get();
    EXPECT_NE(nullptr, aubCSR);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenOsInterfaceIsNullptrThenValidateDrm) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(false, *executionEnvironment, 0));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->osInterface);
    EXPECT_EQ(drmCsr->getOSInterface()->get()->getDrm(), executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->getDrm());
}

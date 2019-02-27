/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/os_interface/linux/device_command_stream.inl"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"
#include "hw_cmds.h"

#include <memory>

using namespace OCLRT;

struct DeviceCommandStreamLeaksTest : ::testing::Test {
    void SetUp() override {
        HardwareInfo *hwInfo = nullptr;
        executionEnvironment = std::unique_ptr<ExecutionEnvironment>(getExecutionEnvironmentImpl(hwInfo));
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

HWTEST_F(DeviceCommandStreamLeaksTest, Create) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false,
                                                                                               *executionEnvironment));
    DrmMockSuccess mockDrm;
    EXPECT_NE(nullptr, ptr);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false,
                                                                                               *executionEnvironment));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_EQ(drmCsr->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWithAubDumWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], true,
                                                                                               *executionEnvironment));
    auto drmCsrWithAubDump = (CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *)ptr.get();
    EXPECT_EQ(drmCsrWithAubDump->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
    auto aubCSR = static_cast<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *>(ptr.get())->aubCSR.get();
    EXPECT_NE(nullptr, aubCSR);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenOsInterfaceIsNullptrThenValidateDrm) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false,
                                                                                               *executionEnvironment));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_NE(nullptr, executionEnvironment->osInterface);
    EXPECT_EQ(drmCsr->getOSInterface()->get()->getDrm(), executionEnvironment->osInterface->get()->getDrm());
}
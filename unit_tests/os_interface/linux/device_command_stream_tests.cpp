/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gtest/gtest.h"
#include <memory>
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/os_interface/linux/device_command_stream.inl"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "hw_cmds.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

using namespace OCLRT;

typedef Test<DeviceFixture> DeviceCommandStreamLeaksTest;

HWTEST_F(DeviceCommandStreamLeaksTest, Create) {
    this->executionEnvironment.osInterface = std::make_unique<OSInterface>();
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false, this->executionEnvironment));
    DrmMockSuccess mockDrm;
    EXPECT_NE(nullptr, ptr);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    this->executionEnvironment.osInterface = std::make_unique<OSInterface>();
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false, this->executionEnvironment));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_EQ(drmCsr->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWithAubDumWhenItIsCreatedThenGemCloseWorkerInactiveModeIsSelected) {
    this->executionEnvironment.osInterface = std::make_unique<OSInterface>();
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], true, this->executionEnvironment));
    auto drmCsrWithAubDump = (CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *)ptr.get();
    EXPECT_EQ(drmCsrWithAubDump->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerActive);
    auto aubCSR = static_cast<CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<FamilyType>> *>(ptr.get())->aubCSR;
    EXPECT_NE(nullptr, aubCSR);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenOsInterfaceIsNullptrThenValidateDrm) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0], false,
                                                                                               this->executionEnvironment));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_NE(nullptr, executionEnvironment.osInterface);
    EXPECT_EQ(drmCsr->getOSInterface()->get()->getDrm(), executionEnvironment.osInterface->get()->getDrm());
}
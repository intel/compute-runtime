/*
 * Copyright (c) 2017, Intel Corporation
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
#include "hw_cmds.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "test.h"

#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

using namespace OCLRT;

class DeviceCommandStreamLeaksFixture : public DeviceFixture,
                                        public MemoryManagementFixture {
  public:
    void SetUp() override {
        DeviceFixture::SetUp();
        MemoryManagementFixture::SetUp();
    }

    void TearDown() override {
        MemoryManagementFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

typedef Test<DeviceCommandStreamLeaksFixture> DeviceCommandStreamLeaksTest;

HWTEST_F(DeviceCommandStreamLeaksTest, Create) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0]));
    DrmMockSuccess mockDrm;
    EXPECT_NE(nullptr, ptr);
}

HWTEST_F(DeviceCommandStreamLeaksTest, givenDefaultDrmCsrWhenItIsCreateThenGemCloseWorkerInactiveModeIsSelected) {
    std::unique_ptr<CommandStreamReceiver> ptr(DeviceCommandStreamReceiver<FamilyType>::create(*platformDevices[0]));
    auto drmCsr = (DrmCommandStreamReceiver<FamilyType> *)ptr.get();
    EXPECT_EQ(drmCsr->peekGemCloseWorkerOperationMode(), gemCloseWorkerMode::gemCloseWorkerInactive);
}

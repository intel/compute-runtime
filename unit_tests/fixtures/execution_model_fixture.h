/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#pragma once

#include "runtime/device_queue/device_queue.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/execution_model_kernel_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

class DeviceQueueFixture {
  public:
    void SetUp(Context *context, Device *device) {
        cl_int errcodeRet = 0;
        cl_queue_properties properties[3];

        properties[0] = CL_QUEUE_PROPERTIES;
        properties[1] = CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
        properties[2] = 0;

        ASSERT_NE(nullptr, context);
        ASSERT_NE(nullptr, device);

        pDevQueue = DeviceQueue::create(context, device,
                                        properties[0],
                                        errcodeRet);

        ASSERT_NE(nullptr, pDevQueue);

        auto devQueue = context->getDefaultDeviceQueue();

        ASSERT_NE(nullptr, devQueue);
        EXPECT_EQ(pDevQueue, devQueue);
    }

    void TearDown() {
        delete pDevQueue;
    }

    DeviceQueue *pDevQueue;
};

class ExecutionModelKernelTest : public ExecutionModelKernelFixture,
                                 public CommandQueueHwFixture,
                                 public DeviceQueueFixture {
  public:
    ExecutionModelKernelTest(){};

    void SetUp() override {
        ExecutionModelKernelFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, 0);
        DeviceQueueFixture::SetUp(context, pDevice);
    }

    void TearDown() override {

        DeviceQueueFixture::TearDown();
        CommandQueueHwFixture::TearDown();
        ExecutionModelKernelFixture::TearDown();
    }
};

class ExecutionModelSchedulerTest : public DeviceFixture,
                                    public CommandQueueHwFixture,
                                    public DeviceQueueFixture {
  public:
    ExecutionModelSchedulerTest(){};

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueHwFixture::SetUp(pDevice, 0);
        DeviceQueueFixture::SetUp(context, pDevice);

        parentKernel = MockParentKernel::create(*context);
        ASSERT_NE(nullptr, parentKernel);
    }

    void TearDown() override {
        delete parentKernel;

        DeviceQueueFixture::TearDown();
        CommandQueueHwFixture::TearDown();
        DeviceFixture::TearDown();
    }

    MockParentKernel *parentKernel;
};

struct ParentKernelCommandQueueFixture : public CommandQueueHwFixture,
                                         testing::Test {

    void SetUp() override {
        device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
        CommandQueueHwFixture::SetUp(device, 0);
    }
    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        BuiltIns::shutDown();
        delete device;
    }
    MockDevice *device;
};

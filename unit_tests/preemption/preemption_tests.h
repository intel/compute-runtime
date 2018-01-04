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

#pragma once

#include "gtest/gtest.h"
#include "test.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_marker.h"

#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/fixtures/hello_world_fixture.h"

namespace OCLRT {
class DevicePreemptionTests : public ::testing::Test {
  public:
    void SetUp() override {
        const cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, 0, 0};
        kernelInfo = KernelInfo::create();
        device = MockDevice::create<OCLRT::MockDevice>(nullptr, false);
        context = new MockContext(device);
        cmdQ = new MockCommandQueue(context, device, properties);
        memset(&executionEnvironment, 0, sizeof(SPatchExecutionEnvironment));
        kernelInfo->patchInfo.executionEnvironment = &executionEnvironment;
        kernel = new MockKernel(&program, *kernelInfo, *device);
        dispatchInfo = new DispatchInfo(kernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));

        ASSERT_NE(nullptr, device);
        ASSERT_NE(nullptr, context);
        ASSERT_NE(nullptr, cmdQ);
        forceWhitelistedRegs(true);
        waTable = const_cast<WorkaroundTable *>(device->getWaTable());
    }
    void TearDown() override {
        if (dbgRestore) {
            delete dbgRestore;
        }
        delete kernel;
        delete kernelInfo;
        delete dispatchInfo;
        delete cmdQ;
        delete context;
        delete device;
    }
    void forceWhitelistedRegs(bool whitelisted) {
        WhitelistedRegisters forceRegs = {whitelisted};
        device->setForceWhitelistedRegs(true, &forceRegs);
    }

    PreemptionMode preemptionMode;
    DispatchInfo *dispatchInfo;
    MockKernel *kernel;
    MockCommandQueue *cmdQ;
    MockDevice *device;
    MockContext *context;
    DebugManagerStateRestore *dbgRestore = nullptr;
    WorkaroundTable *waTable = nullptr;
    SPatchExecutionEnvironment executionEnvironment;
    MockProgram program;
    KernelInfo *kernelInfo;
};

typedef HelloWorldFixture<HelloWorldFixtureFactory> PreemptionEnqueueKernelFixture;
typedef Test<PreemptionEnqueueKernelFixture> PreemptionEnqueueKernelTest;

struct ThreadGroupPreemptionEnqueueKernelTest : PreemptionEnqueueKernelTest {
    void SetUp() override {
        globalHwInfo = const_cast<HardwareInfo *>(platformDevices[0]);
        originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
        globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;

        HelloWorldFixture::SetUp();
        pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    }

    void TearDown() override {
        globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

        HelloWorldFixture::TearDown();
    }

    HardwareInfo *globalHwInfo;
    PreemptionMode originalPreemptionMode;
};

struct MidThreadPreemptionEnqueueKernelTest : PreemptionEnqueueKernelTest {
    void SetUp() override {
        globalHwInfo = const_cast<HardwareInfo *>(platformDevices[0]);
        originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
        globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

        HelloWorldFixture::SetUp();
        pDevice->setPreemptionMode(PreemptionMode::MidThread);
    }

    void TearDown() override {
        globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

        HelloWorldFixture::TearDown();
    }

    HardwareInfo *globalHwInfo;
    PreemptionMode originalPreemptionMode;
};

} // namespace OCLRT

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

#include "unit_tests/fixtures/preemption_fixture.h"

#include "runtime/command_stream/preemption.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/command_queue/enqueue_marker.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"

#include "gtest/gtest.h"
#include "test.h"

using namespace OCLRT;

DevicePreemptionTests::DevicePreemptionTests() = default;
DevicePreemptionTests::~DevicePreemptionTests() = default;

void DevicePreemptionTests::SetUp() {
    if (dbgRestore == nullptr) {
        dbgRestore.reset(new DebugManagerStateRestore());
    }
    const cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    kernelInfo = std::make_unique<KernelInfo>();
    device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    context.reset(new MockContext(device.get()));
    cmdQ.reset(new MockCommandQueue(context.get(), device.get(), properties));
    executionEnvironment.reset(new SPatchExecutionEnvironment);
    memset(executionEnvironment.get(), 0, sizeof(SPatchExecutionEnvironment));
    kernelInfo->patchInfo.executionEnvironment = executionEnvironment.get();
    program = std::make_unique<MockProgram>(*device->getExecutionEnvironment());
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    dispatchInfo.reset(new DispatchInfo(kernel.get(), 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0)));

    ASSERT_NE(nullptr, device);
    ASSERT_NE(nullptr, context);
    ASSERT_NE(nullptr, cmdQ);
    forceWhitelistedRegs(true);
    waTable = const_cast<WorkaroundTable *>(device->getWaTable());
}

void DevicePreemptionTests::TearDown() {
    dbgRestore.reset();
    kernel.reset();
    kernelInfo.reset();
    dispatchInfo.reset();
    cmdQ.reset();
    context.reset();
    device.reset();
}

void DevicePreemptionTests::forceWhitelistedRegs(bool whitelisted) {
    WhitelistedRegisters forceRegs = {whitelisted};
    device->setForceWhitelistedRegs(true, &forceRegs);
}

void ThreadGroupPreemptionEnqueueKernelTest::SetUp() {
    dbgRestore.reset(new DebugManagerStateRestore());
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    globalHwInfo = const_cast<HardwareInfo *>(platformDevices[0]);
    originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
    globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;

    HelloWorldFixture::SetUp();
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
}

void ThreadGroupPreemptionEnqueueKernelTest::TearDown() {
    globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

    HelloWorldFixture::TearDown();
}

void MidThreadPreemptionEnqueueKernelTest::SetUp() {
    dbgRestore.reset(new DebugManagerStateRestore());
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    globalHwInfo = const_cast<HardwareInfo *>(platformDevices[0]);
    originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
    globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    HelloWorldFixture::SetUp();
    pDevice->setPreemptionMode(PreemptionMode::MidThread);
}

void MidThreadPreemptionEnqueueKernelTest::TearDown() {
    globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

    HelloWorldFixture::TearDown();
}

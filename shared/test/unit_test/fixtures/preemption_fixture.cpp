/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/command_queue/enqueue_kernel.h"
#include "opencl/source/command_queue/enqueue_marker.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/scheduler/scheduler_kernel.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

DevicePreemptionTests::DevicePreemptionTests() = default;
DevicePreemptionTests::~DevicePreemptionTests() = default;

void DevicePreemptionTests::SetUp() {
    if (dbgRestore == nullptr) {
        dbgRestore.reset(new DebugManagerStateRestore());
    }
    const cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    kernelInfo = std::make_unique<KernelInfo>();
    device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
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

    waTable = &device->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;
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

void ThreadGroupPreemptionEnqueueKernelTest::SetUp() {
    dbgRestore.reset(new DebugManagerStateRestore());
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    globalHwInfo = defaultHwInfo.get();
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

    globalHwInfo = defaultHwInfo.get();
    originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
    globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    HelloWorldFixture::SetUp();
    pDevice->setPreemptionMode(PreemptionMode::MidThread);
}

void MidThreadPreemptionEnqueueKernelTest::TearDown() {
    globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

    HelloWorldFixture::TearDown();
}

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

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
    device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
    context.reset(new MockContext(device.get()));
    cmdQ.reset(new MockCommandQueue(context.get(), device.get(), properties, false));
    program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    kernel.reset(new MockKernel(program.get(), *kernelInfo, *device));
    dispatchInfo.reset(new DispatchInfo(device.get(), kernel.get(), 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0)));

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
    program.reset();
    cmdQ.reset();
    context.reset();
    device.reset();
}

void ThreadGroupPreemptionEnqueueKernelTest::SetUp() {
    dbgRestore.reset(new DebugManagerStateRestore());
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    globalHwInfo = defaultHwInfo.get();
    originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
    globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;

    HelloWorldFixture::setUp();
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
}

void ThreadGroupPreemptionEnqueueKernelTest::TearDown() {
    globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

    HelloWorldFixture::tearDown();
}

void MidThreadPreemptionEnqueueKernelTest::SetUp() {
    dbgRestore.reset(new DebugManagerStateRestore());
    debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

    globalHwInfo = defaultHwInfo.get();
    originalPreemptionMode = globalHwInfo->capabilityTable.defaultPreemptionMode;
    globalHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;

    HelloWorldFixture::setUp();
    pDevice->setPreemptionMode(PreemptionMode::MidThread);
}

void MidThreadPreemptionEnqueueKernelTest::TearDown() {
    globalHwInfo->capabilityTable.defaultPreemptionMode = originalPreemptionMode;

    HelloWorldFixture::tearDown();
}

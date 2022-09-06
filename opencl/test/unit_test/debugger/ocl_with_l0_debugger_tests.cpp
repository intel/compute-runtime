/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct DebuggerClFixture
    : public ContextFixture,
      public CommandQueueHwFixture {

    void setUp() {
        hardwareInfo = *NEO::defaultHwInfo.get();
        auto executionEnvironment = MockClDevice::prepareExecutionEnvironment(&hardwareInfo, rootDeviceIndex);
        executionEnvironment->setDebuggingEnabled();
        device = MockDevice::createWithExecutionEnvironment<MockDevice>(&hardwareInfo, executionEnvironment, rootDeviceIndex);
        ASSERT_NE(nullptr, device);
        clExecutionEnvironment = static_cast<MockClExecutionEnvironment *>(device->getExecutionEnvironment());
        clDevice = new MockClDevice{device};
        ASSERT_NE(nullptr, clDevice);

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initDebuggerL0(device);
        this->osContext = device->getDefaultEngine().osContext;

        cl_device_id deviceId = clDevice;
        ContextFixture::setUp(1, &deviceId);
        CommandQueueHwFixture::setUp(clDevice, 0);
    }

    void tearDown() {
        ContextFixture::tearDown();
        CommandQueueHwFixture::tearDown();
        delete clDevice;
        clDevice = nullptr;
        device = nullptr;
    }

    MockDevice *device = nullptr;
    MockClDevice *clDevice = nullptr;
    HardwareInfo hardwareInfo = {};
    OsContext *osContext = nullptr;
    const uint32_t rootDeviceIndex = 0u;
    MockClExecutionEnvironment *clExecutionEnvironment = nullptr;
};

using DebuggerCmdQTest = Test<DebuggerClFixture>;

HWTEST_F(DebuggerCmdQTest, GivenDebuggingEnabledWhenCommandQueueIsCreatedAndReleasedThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->commandQueueCreatedCount = 0;
    debuggerL0Hw->commandQueueDestroyedCount = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, clDevice, 0);

    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    cl_int retVal = 0;
    releaseQueue(mockCmdQ, retVal);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(DebuggerCmdQTest, GivenDebuggingEnabledWhenInternalCommandQueueIsCreatedAndReleasedThenDebuggerL0IsNotNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->commandQueueCreatedCount = 0;
    debuggerL0Hw->commandQueueDestroyedCount = 0;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, clDevice, 0, true);

    EXPECT_EQ(0u, debuggerL0Hw->commandQueueCreatedCount);
    cl_int retVal = 0;
    releaseQueue(mockCmdQ, retVal);
    EXPECT_EQ(0u, debuggerL0Hw->commandQueueDestroyedCount);
}

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;
HWTEST2_F(DebuggerCmdQTest, GivenDebuggingEnabledWhenEnqueueingKernelThenDebugSurfaceIsResident, Gen12Plus) {
    int32_t executionStamp = 0;
    auto mockCSR = new MockCsr<FamilyType>(executionStamp, *device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    device->resetCommandStreamReceiver(mockCSR);

    MockKernelWithInternals mockKernelWithInternals(*clDevice);
    auto mockKernel = mockKernelWithInternals.mockKernel;

    size_t offset = 0;
    size_t size = 1;
    pCmdQ->initializeGpgpu();
    pCmdQ->enqueueKernel(mockKernel, 1, &offset, &size, &size, 0, nullptr, nullptr);

    EXPECT_TRUE(mockCSR->isMadeResident(mockCSR->getDebugSurfaceAllocation()));
}
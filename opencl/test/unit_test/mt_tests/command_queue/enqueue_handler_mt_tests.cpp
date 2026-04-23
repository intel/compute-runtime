/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <memory>
#include <thread>

using namespace NEO;

struct EnqueueHandlerTestBasicMt : public ::testing::Test {
    template <typename MockCmdQueueType, typename FamilyType>
    std::unique_ptr<MockCmdQueueType> setupFixtureAndCreateMockCommandQueue() {
        auto executionEnvironment = platform()->peekExecutionEnvironment();

        device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, executionEnvironment, 0u));
        context = std::make_unique<MockContext>(device.get());

        auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(device->getGpgpuCommandStreamReceiver());
        mockInternalAllocationStorage = new MockInternalAllocationStorage(ultCsr);
        ultCsr.internalAllocationStorage.reset(mockInternalAllocationStorage);

        auto mockCmdQ = std::make_unique<MockCmdQueueType>(context.get(), device.get(), nullptr);

        ultCsr.taskCount = initialTaskCount;

        return mockCmdQ;
    }
    MockInternalAllocationStorage *mockInternalAllocationStorage = nullptr;
    const uint32_t initialTaskCount = 100;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(EnqueueHandlerTestBasicMt, givenBlockedEnqueueHandlerWhenCommandIsBlockingThenCompletionStampTaskCountIsPassedToWaitForTaskCountAndCleanAllocationListAsRequiredTaskCount) {
    auto mockCmdQ = setupFixtureAndCreateMockCommandQueue<MockCommandQueueHw<FamilyType>, FamilyType>();

    MockKernelWithInternals kernelInternals(*context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel);

    UserEvent userEvent;
    cl_event waitlist[] = {&userEvent};

    std::thread t0([&mockCmdQ, &userEvent]() {
        while (!mockCmdQ->isQueueBlocked()) {
        }
        userEvent.setStatus(CL_COMPLETE);
    });
    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_WRITE_BUFFER>(nullptr,
                                                                                          0,
                                                                                          true,
                                                                                          multiDispatchInfo,
                                                                                          1,
                                                                                          waitlist,
                                                                                          nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);
    EXPECT_EQ(initialTaskCount + 1, mockInternalAllocationStorage->lastCleanAllocationsTaskCount);

    t0.join();
}

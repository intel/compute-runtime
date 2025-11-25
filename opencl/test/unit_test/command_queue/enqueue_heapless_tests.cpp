/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/event_builder.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

namespace NEO {
class Kernel;
} // namespace NEO

HWTEST2_F(EnqueueHandlerTest, givenCommandStreamWithoutKernelAndHeaplessStateInitEnabledWhenCommandEnqueuedThenTaskCountIncreased, HeaplessSupport) {

    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));
    mockCmdQ->heaplessModeEnabled = true;
    mockCmdQ->heaplessStateInitEnabled = true;

    auto initialTaskCount = std::max(mockCmdQ->getGpgpuCommandStreamReceiver().peekTaskCount(), mockCmdQ->taskCount);

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    TimestampPacketDependencies timestampPacketDependencies;

    CsrDependencies csrDeps;
    EnqueueProperties enqueueProperties(false, false, false, true, false, false, nullptr);

    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, &mockCmdQ->getCS(0), 0, blocking, enqueueProperties, timestampPacketDependencies,
                                          eventsRequest, eventBuilder, 0, csrDeps, nullptr, false);
    EXPECT_EQ(allocation->getTaskCount(mockCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getContextId()), initialTaskCount + 1u);
}

HWTEST2_F(EnqueueHandlerTest, givenNonBlockingAndHeaplessWhenEnqueueHandlerNdRangeIsCalledThenSuccessIsReturned, HeaplessSupport) {

    MockKernelWithInternals kernelInternals(*pClDevice, context);
    Kernel *kernel = kernelInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel);

    auto mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pClDevice, 0);

    mockCmdQ->heaplessModeEnabled = true;
    mockCmdQ->heaplessStateInitEnabled = true;

    const auto enqueueResult = mockCmdQ->template enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(nullptr,
                                                                                            0,
                                                                                            false,
                                                                                            multiDispatchInfo,
                                                                                            0,
                                                                                            nullptr,
                                                                                            nullptr);
    EXPECT_EQ(CL_SUCCESS, enqueueResult);

    mockCmdQ->release();
}

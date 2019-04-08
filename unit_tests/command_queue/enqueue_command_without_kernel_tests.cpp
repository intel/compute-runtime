/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
class MockCommandQueueWithCacheFlush : public MockCommandQueueHw<GfxFamily> {
    using MockCommandQueueHw<GfxFamily>::MockCommandQueueHw;

  public:
    bool isCacheFlushCommand(uint32_t commandType) override {
        return commandRequireCacheFlush;
    }
    bool commandRequireCacheFlush = false;
};

HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelWhenCommandEnqueuedThenTaskCountIncreased) {

    std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(context, pDevice, 0));

    char buffer[64];
    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, sizeof(buffer)));
    std::unique_ptr<GeneralSurface> surface(new GeneralSurface(allocation.get()));
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;
    Surface *surfaces[] = {surface.get()};
    auto blocking = true;
    mockCmdQ->enqueueCommandWithoutKernel(surfaces, 1, mockCmdQ->getCS(0), 0, blocking, nullptr, eventsRequest, eventBuilder, 0);
    EXPECT_EQ(allocation->getTaskCount(mockCmdQ->getCommandStreamReceiver().getOsContext().getContextId()), 1u);
}
HWTEST_F(EnqueueHandlerTest, GivenCommandStreamWithoutKernelAndZeroSurfacesWhenEnqueuedHandlerThenUsedSizeEqualZero) {

    std::unique_ptr<MockCommandQueueWithCacheFlush<FamilyType>> mockCmdQ(new MockCommandQueueWithCacheFlush<FamilyType>(context, pDevice, 0));

    mockCmdQ->commandRequireCacheFlush = true;
    mockCmdQ->template enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, false, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(mockCmdQ->getCS(0).getUsed(), 0u);
}
} // namespace NEO

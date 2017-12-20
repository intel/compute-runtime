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

#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "test.h"

using namespace OCLRT;

struct CommandQueueMemoryDevice
    : public MemoryManagementFixture,
      public DeviceFixture {

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        MemoryManagementFixture::TearDown();
    }
};

struct CommandQueueTest
    : public CommandQueueMemoryDevice,
      public ContextFixture,
      public CommandQueueFixture,
      ::testing::TestWithParam<uint64_t /*cl_command_queue_properties*/> {

    using CommandQueueFixture::SetUp;
    using ContextFixture::SetUp;

    CommandQueueTest() {
    }

    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        properties = GetParam();

        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        CommandQueueFixture::SetUp(pContext, pDevice, properties);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        ContextFixture::TearDown();
        CommandQueueMemoryDevice::TearDown();
    }

    cl_command_queue_properties properties;
    const HardwareInfo *pHwInfo = nullptr;
};

TEST_P(CommandQueueTest, createDeleteCommandQueue_Properties) {
    InjectedFunction method = [this](size_t failureIndex) {
        auto retVal = CL_INVALID_VALUE;
        auto pCmdQ = CommandQueue::create(
            pContext,
            pDevice,
            nullptr,
            retVal);

        if (nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, pCmdQ);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, pCmdQ);
        }
        delete pCmdQ;
    };
    injectFailures(method);
}

INSTANTIATE_TEST_CASE_P(CommandQueue,
                        CommandQueueTest,
                        ::testing::ValuesIn(AllCommandQueueProperties));

TEST(CommandQueue, taskLevelInitializesTo0) {
    CommandQueue cmdQ(nullptr, nullptr, 0);
    EXPECT_EQ(0u, cmdQ.taskLevel);
    EXPECT_EQ(0u, cmdQ.taskCount);
}

struct GetTagTest : public DeviceFixture,
                    public CommandQueueFixture,
                    public CommandStreamFixture,
                    public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pDevice, 0);
        CommandStreamFixture::SetUp(pCmdQ);
    }

    void TearDown() override {
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

TEST_F(GetTagTest, shouldReturnValue) {
    uint32_t tagValue = 0xdeadbeef;
    *pTagMemory = tagValue;
    EXPECT_EQ(tagValue, pCmdQ->getHwTag());
}

TEST_F(GetTagTest, getHwTagInitialValue) {
    MockContext context;
    CommandQueue commandQueue(&context, pDevice, 0);

    EXPECT_EQ(initialHardwareTag, commandQueue.getHwTag());
}

TEST(CommandQueue, IOQ_taskLevelFromCompletionStamp) {
    MockContext context;

    CommandQueue cmdQ(&context, nullptr, 0);

    CompletionStamp cs = {
        cmdQ.taskCount + 100,
        cmdQ.taskLevel + 50,
        5,
        0,
        EngineType::ENGINE_RCS};
    cmdQ.updateFromCompletionStamp(cs);

    EXPECT_EQ(cs.taskLevel, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, GivenOOQwhenUpdateFromCompletionStampWithTrueIsCalledThenTaskLevelIsUpdated) {
    MockContext context;
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};

    CommandQueue cmdQ(&context, nullptr, props);
    auto oldTL = cmdQ.taskLevel;

    CompletionStamp cs = {
        cmdQ.taskCount + 100,
        cmdQ.taskLevel + 50,
        5,
        0,
        EngineType::ENGINE_RCS};
    cmdQ.updateFromCompletionStamp(cs);

    EXPECT_NE(oldTL, cmdQ.taskLevel);
    EXPECT_EQ(oldTL + 50, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenCmdQueueBlockedByReadyVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);
    Event userEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent.setStatus(CL_COMPLETE);
    userEvent.flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;
    EXPECT_EQ(expectedFlushStamp, cmdQ.flushStamp->peekStamp());
    userEvent.incRefInternal();
    cmdQ.virtualEvent = &userEvent;
    cmdQ.incRefInternal();

    EXPECT_FALSE(cmdQ.isQueueBlocked());
    EXPECT_EQ(userEvent.flushStamp->peekStamp(), cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenCmdQueueBlockedByAbortedVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    MockContext context;
    std::unique_ptr<MockDevice> mockDevice(Device::create<MockDevice>(nullptr));
    CommandQueue cmdQ(&context, mockDevice.get(), 0);

    Event userEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent.setStatus(-1);
    userEvent.flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;

    EXPECT_EQ(expectedFlushStamp, cmdQ.flushStamp->peekStamp());
    userEvent.incRefInternal();
    cmdQ.virtualEvent = &userEvent;
    cmdQ.incRefInternal();

    EXPECT_FALSE(cmdQ.isQueueBlocked());
    EXPECT_EQ(expectedFlushStamp, cmdQ.flushStamp->peekStamp());
}

struct CommandQueueCommandStreamTest : public CommandQueueMemoryDevice,
                                       public ::testing::Test {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
    }

    void TearDown() override {
        CommandQueueMemoryDevice::TearDown();
    }
    MockContext context;
};

HWTEST_F(CommandQueueCommandStreamTest, givenCommandQueueThatWaitsOnAbortedUserEventWhenIsQueueBlockedIsCalledThenTaskLevelAlignsToCsr) {
    MockContext context;
    std::unique_ptr<MockDevice> mockDevice(Device::create<MockDevice>(nullptr));

    CommandQueue cmdQ(&context, mockDevice.get(), 0);
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskLevel = 100u;

    Event userEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent.setStatus(-1);
    userEvent.incRefInternal();
    cmdQ.virtualEvent = &userEvent;
    cmdQ.incRefInternal();

    EXPECT_FALSE(cmdQ.isQueueBlocked());
    EXPECT_EQ(100u, cmdQ.taskLevel);
}

TEST_F(CommandQueueCommandStreamTest, givenCmdQueueBlockedByEventWhenEventIsAsynchronouslyCompletedThenBroadcastUpdateAllIsNotCalledOnIsQueueBlockedCall) {
    MockContext context;
    CommandQueue cmdQ(&context, pDevice, 0);

    class MockEventWithSetCompleteOnUpdate : public Event {
      public:
        MockEventWithSetCompleteOnUpdate(CommandQueue *cmdQueue, cl_command_type cmdType,
                                         uint32_t taskLevel, uint32_t taskCount) : Event(cmdQueue, cmdType, taskLevel, taskCount) {
        }
        void updateExecutionStatus() override {
            setStatus(CL_COMPLETE);
        }
    };

    MockEventWithSetCompleteOnUpdate *event = new MockEventWithSetCompleteOnUpdate(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    event->setStatus(CL_SUBMITTED);

    Event virtualEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 20);

    event->addChild(virtualEvent);

    // Put Queue in blocked state by assigning virtualEvent
    virtualEvent.incRefInternal();
    cmdQ.virtualEvent = &virtualEvent;
    cmdQ.incRefInternal();

    context.getEventsRegistry().registerEvent(*event);

    EXPECT_TRUE(cmdQ.isQueueBlocked());
    // This second check is purposely here, queue shouldn't be unblocked asynchrounously in isQueueBlocked
    // because it can lead to deadlock when isQueueBlocked() is called under device mutex and would try to
    // acquire registry mutex, while second thread owns mutex for registry processing.
    EXPECT_TRUE(cmdQ.isQueueBlocked());

    context.getEventsRegistry().broadcastUpdateAll();
    context.getEventsRegistry().dropEvent(*event);

    event->release();
}

TEST_F(CommandQueueCommandStreamTest, GetCommandStreamReturnsValidObject) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(&context, pDevice, props);

    auto &cs = commandQueue.getCS();
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandQueueCommandStreamTest, GetCommandStreamReturnsCsWithCsOverfetchSizeIncludedInGraphicsAllocation) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(&context, pDevice, props);
    size_t minSizeRequested = 20;

    auto &cs = commandQueue.getCS(minSizeRequested);
    ASSERT_NE(nullptr, &cs);

    auto *allocation = cs.getGraphicsAllocation();
    ASSERT_NE(nullptr, &allocation);
    size_t expectedCsSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize, MemoryConstants::pageSize) - CSRequirements::minCommandQueueCommandStreamSize;
    EXPECT_EQ(expectedCsSize, cs.getMaxAvailableSpace());

    size_t expectedTotalSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize, MemoryConstants::pageSize) + CSRequirements::csOverfetchSize;
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandQueueCommandStreamTest, getCommandStreamContainsMemoryForRequest) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(&context, pDevice, props);

    size_t requiredSize = 16384;
    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, getCommandStreamCanRecycle) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(&context, pDevice, props);

    auto &commandStreamInitial = commandQueue.getCS();
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, MemoryManagerWithReusableAllocationsWhenAskedForCommandStreamReturnsAllocationFromReusablePool) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    size_t requiredSize = alignUp(100, MemoryConstants::pageSize) + CSRequirements::csOverfetchSize;
    auto allocation = memoryManager->allocateGraphicsMemory(requiredSize, 4096);
    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());
}
TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenItIsDestroyedThenCommandStreamIsPutOnTheReusabeList) {
    auto cmdQ = new CommandQueue(&context, pDevice, 0);
    auto memoryManager = pDevice->getMemoryManager();
    const auto &commandStream = cmdQ->getCS(100);
    auto graphicsAllocation = commandStream.getGraphicsAllocation();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, CommandQueueWhenAskedForNewCommandStreamStoresOldHeapForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    cmdQ.getCS(10000);

    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*graphicsAllocation));
}

struct CommandQueueIndirectHeapTest : public CommandQueueMemoryDevice,
                                      public ::testing::TestWithParam<IndirectHeap::Type> {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
    }

    void TearDown() override {
        CommandQueueMemoryDevice::TearDown();
    }
    MockContext context;
};

TEST_P(CommandQueueIndirectHeapTest, IndirectHeapIsProvidedByDevice) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 8192);
    EXPECT_NE(nullptr, &indirectHeap);
}

TEST_P(CommandQueueIndirectHeapTest, IndirectHeapContainsAtLeast64KB) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), sizeof(uint32_t));
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        EXPECT_EQ(64 * KB - MemoryConstants::pageSize, indirectHeap.getAvailableSpace());
    } else {
        EXPECT_EQ(64 * KB, indirectHeap.getAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, getIndirectHeapContainsMemoryForRequest) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    size_t requiredSize = 16384;
    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_GE(indirectHeap.getMaxAvailableSpace(), requiredSize);
}

TEST_P(CommandQueueIndirectHeapTest, getIndirectHeapCanRecycle) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 10);
    size_t requiredSize = indirectHeapInitial.getMaxAvailableSpace() + 42;

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_GE(indirectHeap.getMaxAvailableSpace(), requiredSize);
}

TEST_P(CommandQueueIndirectHeapTest, alignSizeToCacheLine) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);
    size_t minHeapSize = 64 * KB;

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 2 * minHeapSize + 1);

    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeapInitial.getAvailableSpace()));

    indirectHeapInitial.getSpace(indirectHeapInitial.getAvailableSpace()); // use whole space to force obtain reusable

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), minHeapSize + 1);

    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeap.getAvailableSpace()));
}

TEST_P(CommandQueueIndirectHeapTest, MemoryManagerWithReusableAllocationsWhenAskedForHeapAllocationReturnsAllocationFromReusablePool) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    auto allocation = memoryManager->allocateGraphicsMemory(128 * KB, 4096);
    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    //make sure we are below 64 KB even though we reuse 128KB.
    EXPECT_LE(indirectHeap.getMaxAvailableSpace(), 64 * KB);

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());
}

TEST_P(CommandQueueIndirectHeapTest, CommandQueueWhenAskedForNewHeapStoresOldHeapForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());

    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*graphicsAllocation));
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocationWhenAskedForNewHeapReturnsAcquiresNewAllocationWithoutStoring) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    cmdQ.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    cmdQ.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_NE(graphicsAllocation, indirectHeap.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWithResourceCachingActiveWhenQueueISDestroyedThenIndirectHeapIsPutOnReuseList) {
    auto cmdQ = new CommandQueue(&context, pDevice, 0);
    auto memoryManager = pDevice->getMemoryManager();
    const auto &indirectHeap = cmdQ->getIndirectHeap(this->GetParam(), 100);
    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_FALSE(memoryManager->allocationsForReuse.peekIsEmpty());
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekContains(*graphicsAllocation));
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapAllocatedWhenIndirectHeapIsReleasedThenHeapAllocationAndHeapBufferIsSetToNullptr) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(&context, pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getMaxAvailableSpace();

    EXPECT_NE(0u, heapSize);

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();
    EXPECT_NE(nullptr, graphicsAllocation);

    cmdQ.releaseIndirectHeap(this->GetParam());

    EXPECT_EQ(nullptr, cmdQ.indirectHeap[this->GetParam()]->getGraphicsAllocation());

    EXPECT_EQ(nullptr, indirectHeap.getBase());
    EXPECT_EQ(0u, indirectHeap.getMaxAvailableSpace());
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocatedWhenIndirectHeapIsReleasedThenIndirectHeapAllocationStaysNull) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(&context, pDevice, props);

    cmdQ.releaseIndirectHeap(this->GetParam());

    EXPECT_EQ(nullptr, cmdQ.indirectHeap[this->GetParam()]);
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapWhenGraphicAllocationIsNullThenNothingOnReuseList) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(&context, pDevice, props);

    auto &ih = cmdQ.getIndirectHeap(this->GetParam());
    auto allocation = ih.getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);

    cmdQ.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    cmdQ.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    cmdQ.releaseIndirectHeap(this->GetParam());

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(memoryManager->allocationsForReuse.peekIsEmpty());

    memoryManager->freeGraphicsMemory(allocation);
}

INSTANTIATE_TEST_CASE_P(
    Device,
    CommandQueueIndirectHeapTest,
    testing::Values(
        IndirectHeap::DYNAMIC_STATE,
        IndirectHeap::GENERAL_STATE,
        IndirectHeap::INDIRECT_OBJECT,
        IndirectHeap::INSTRUCTION,
        IndirectHeap::SURFACE_STATE));

typedef Test<DeviceFixture> CommandQueueCSTest;
HWTEST_F(CommandQueueCSTest, getCSShouldReturnACSWithEnoughSizeCSRTraffic) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pDevice, 0);

    auto WaNeeded = KernelCommandsHelper<FamilyType>::isPipeControlWArequired();

    size_t sizeCSRNeeds =
        sizeof(typename FamilyType::PIPE_CONTROL) * (WaNeeded ? 2 : 1) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END) + MemoryConstants::cacheLineSize - 1;

    sizeCSRNeeds = alignUp(sizeCSRNeeds, MemoryConstants::cacheLineSize);

    // NOTE: This test attempts to reserve the maximum amount
    // of memory such that if a client gets everything he wants
    // we don't overflow/corrupt memory when CSR appends its
    // work.
    size_t sizeCQReserves = CSRequirements::minCommandQueueCommandStreamSize;

    EXPECT_EQ(sizeCSRNeeds, sizeCQReserves);

    size_t sizeRequested = 0x1000 - sizeCQReserves;
    auto &cs = commandQueue.getCS(sizeRequested);
    ASSERT_GE(0x1000u, cs.getMaxAvailableSpace());

    EXPECT_GE(cs.getAvailableSpace(), sizeRequested);
    cs.getSpace(sizeRequested);

    // This should trigger an assert.
    //cs.getSpace(sizeCSRNeeds);

    // This won't trigger an assert. CSR should use
    // this interface for CS's it doesn't own.
    cs.getSpaceUnsecure(sizeCSRNeeds);
    EXPECT_EQ(0x1000u, cs.getUsed());
}

struct KmdNotifyTests : public ::testing::Test {

    void SetUp() override {
        resetObjects(1, 1);
        *device->getTagAddress() = taskCountToWait;
    }

    void TearDown() override {
        delete cmdQ;
        delete device;
        DeviceFactory::releaseDevices();
    }

    void resetObjects(int32_t overrideEnable, int32_t overrideTimeout) {
        if (cmdQ) {
            delete cmdQ;
        }
        if (device) {
            delete device;
            DeviceFactory::releaseDevices();
        }
        DebugManagerStateRestore stateRestore;
        DebugManager.flags.OverrideEnableKmdNotify.set(overrideEnable);
        DebugManager.flags.OverrideKmdNotifyDelayMs.set(overrideTimeout);
        size_t numDevices;
        HardwareInfo *hwInfo = nullptr;
        DeviceFactory::getDevices(&hwInfo, numDevices);
        device = Device::create<MockDevice>(hwInfo);
        cmdQ = new ::testing::NiceMock<MyCommandQueue>(&context, device);
        device->getCommandStreamReceiver().waitForFlushStamp(flushStampToWait);
    }

    class MyCommandQueue : public CommandQueue {
      public:
        MyCommandQueue(Context *ctx, Device *device) : CommandQueue(ctx, device, 0) {}
    };

    template <typename Family>
    class MyCsr : public UltCommandStreamReceiver<Family> {
      public:
        MyCsr(const HardwareInfo &hwInfo) : UltCommandStreamReceiver<Family>(hwInfo) {}
        MOCK_METHOD1(waitForFlushStamp, bool(FlushStamp &flushStampToWait));
        MOCK_METHOD3(waitForCompletionWithTimeout, bool(bool enableTimeout, int64_t timeoutMs, uint32_t taskCountToWait));
    };

    MockContext context;
    MockDevice *device = nullptr;
    ::testing::NiceMock<MyCommandQueue> *cmdQ = nullptr;
    FlushStamp flushStampToWait = 1000;
    uint32_t taskCountToWait = 5;
};

HWTEST_F(KmdNotifyTests, givenTaskCountWhenWaitUntilCompletionCalledThenAlwaysTryCpuPolling) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 1, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 1, taskCountToWait)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait);
}

HWTEST_F(KmdNotifyTests, givenTaskCountAndKmdNotifyDisabledWhenWaitUntilCompletionCalledThenTryCpuPollingWithoutTimeout) {
    resetObjects(0, 0);
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 0, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndKmdWait) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);
    *device->getTagAddress() = taskCountToWait - 1;

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 1, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));
    EXPECT_CALL(*csr, waitForFlushStamp(flushStampToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(false, 1, taskCountToWait)).Times(1).WillOnce(::testing::Return(false));

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait);
}

HWTEST_F(KmdNotifyTests, givenReadyTaskCountWhenWaitUntilCompletionCalledThenTryCpuPollingAndDontCallKmdWait) {
    auto csr = new ::testing::NiceMock<MyCsr<FamilyType>>(device->getHardwareInfo());
    device->resetCommandStreamReceiver(csr);

    ::testing::InSequence is;
    EXPECT_CALL(*csr, waitForCompletionWithTimeout(true, 1, taskCountToWait)).Times(1).WillOnce(::testing::Return(true));
    EXPECT_CALL(*csr, waitForFlushStamp(::testing::_)).Times(0);

    cmdQ->waitUntilComplete(taskCountToWait, flushStampToWait);
}

HWTEST_F(KmdNotifyTests, givenNotReadyTaskCountWhenPollForCompletionCalledThenTimeout) {
    CommandQueue commandQ(&context, device, 0);
    *device->getTagAddress() = taskCountToWait - 1;
    auto success = device->getCommandStreamReceiver().waitForCompletionWithTimeout(true, 1, taskCountToWait);
    EXPECT_FALSE(success);
}

HWTEST_F(KmdNotifyTests, givenMultipleCommandQueuesWhenMarkerIsEmittedThenGraphicsAllocationIsReused) {
    std::unique_ptr<CommandQueue> commandQ(new CommandQueue(&context, device, 0));
    *device->getTagAddress() = 0;
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);

    auto commandStreamGraphicsAllocation = commandQ->getCS(0).getGraphicsAllocation();
    commandQ.reset(new CommandQueue(&context, device, 0));
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    auto commandStreamGraphicsAllocation2 = commandQ->getCS(0).getGraphicsAllocation();
    EXPECT_EQ(commandStreamGraphicsAllocation, commandStreamGraphicsAllocation2);
}

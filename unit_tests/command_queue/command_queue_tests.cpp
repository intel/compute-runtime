/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/basic_math.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/event.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_program.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace NEO;

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
        5};
    cmdQ.updateFromCompletionStamp(cs);

    EXPECT_EQ(cs.taskLevel, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenTimeStampWithTaskCountNotReadyStatusWhenupdateFromCompletionStampIsBeingCalledThenQueueTaskCountIsNotUpdated) {
    MockContext context;

    CommandQueue cmdQ(&context, nullptr, 0);

    cmdQ.taskCount = 1u;

    CompletionStamp cs = {
        Event::eventNotReady,
        0,
        0};
    cmdQ.updateFromCompletionStamp(cs);
    EXPECT_EQ(1u, cmdQ.taskCount);
}

TEST(CommandQueue, GivenOOQwhenUpdateFromCompletionStampWithTrueIsCalledThenTaskLevelIsUpdated) {
    MockContext context;
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};

    CommandQueue cmdQ(&context, nullptr, props);
    auto oldTL = cmdQ.taskLevel;

    CompletionStamp cs = {
        cmdQ.taskCount + 100,
        cmdQ.taskLevel + 50,
        5};
    cmdQ.updateFromCompletionStamp(cs);

    EXPECT_NE(oldTL, cmdQ.taskLevel);
    EXPECT_EQ(oldTL + 50, cmdQ.taskLevel);
    EXPECT_EQ(cs.taskCount, cmdQ.taskCount);
    EXPECT_EQ(cs.flushStamp, cmdQ.flushStamp->peekStamp());
}

TEST(CommandQueue, givenDeviceWhenCreatingCommandQueueThenPickCsrFromDefaultEngine) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    CommandQueue cmdQ(nullptr, mockDevice.get(), 0);

    auto defaultCsr = mockDevice->getDefaultEngine().commandStreamReceiver;
    EXPECT_EQ(defaultCsr, &cmdQ.getCommandStreamReceiver());
}

TEST(CommandQueue, givenCmdQueueBlockedByReadyVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = new MockContext;
    auto cmdQ = new CommandQueue(context, mockDevice.get(), 0);
    auto userEvent = new Event(cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent->setStatus(CL_COMPLETE);
    userEvent->flushStamp->setStamp(5);
    userEvent->incRefInternal();

    FlushStamp expectedFlushStamp = 0;
    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    cmdQ->virtualEvent = userEvent;

    EXPECT_FALSE(cmdQ->isQueueBlocked());
    EXPECT_EQ(userEvent->flushStamp->peekStamp(), cmdQ->flushStamp->peekStamp());
    userEvent->decRefInternal();
    cmdQ->decRefInternal();
    context->decRefInternal();
}

TEST(CommandQueue, givenCmdQueueBlockedByAbortedVirtualEventWhenUnblockingThenUpdateFlushTaskFromEvent) {
    auto context = new MockContext;
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto cmdQ = new CommandQueue(context, mockDevice.get(), 0);

    auto userEvent = new Event(cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent->setStatus(-1);
    userEvent->flushStamp->setStamp(5);

    FlushStamp expectedFlushStamp = 0;

    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    userEvent->incRefInternal();
    cmdQ->virtualEvent = userEvent;

    EXPECT_FALSE(cmdQ->isQueueBlocked());
    EXPECT_EQ(expectedFlushStamp, cmdQ->flushStamp->peekStamp());
    userEvent->decRefInternal();
    cmdQ->decRefInternal();
    context->decRefInternal();
}

struct CommandQueueCommandStreamTest : public CommandQueueMemoryDevice,
                                       public ::testing::Test {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        context.reset(new MockContext(pDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::TearDown();
    }
    std::unique_ptr<MockContext> context;
};

HWTEST_F(CommandQueueCommandStreamTest, givenCommandQueueThatWaitsOnAbortedUserEventWhenIsQueueBlockedIsCalledThenTaskLevelAlignsToCsr) {
    MockContext context;
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    CommandQueue cmdQ(&context, mockDevice.get(), 0);
    auto &commandStreamReceiver = mockDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.taskLevel = 100u;

    Event userEvent(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 0, 0);
    userEvent.setStatus(-1);
    userEvent.incRefInternal();
    cmdQ.virtualEvent = &userEvent;

    EXPECT_FALSE(cmdQ.isQueueBlocked());
    EXPECT_EQ(100u, cmdQ.taskLevel);
}

TEST_F(CommandQueueCommandStreamTest, GetCommandStreamReturnsValidObject) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(context.get(), pDevice, props);

    auto &cs = commandQueue.getCS(1024);
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandQueueCommandStreamTest, GetCommandStreamReturnsCsWithCsOverfetchSizeIncludedInGraphicsAllocation) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(context.get(), pDevice, props);
    size_t minSizeRequested = 20;

    auto &cs = commandQueue.getCS(minSizeRequested);
    ASSERT_NE(nullptr, &cs);

    auto *allocation = cs.getGraphicsAllocation();
    ASSERT_NE(nullptr, &allocation);
    size_t expectedCsSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k) - CSRequirements::minCommandQueueCommandStreamSize - CSRequirements::csOverfetchSize;
    EXPECT_EQ(expectedCsSize, cs.getMaxAvailableSpace());

    size_t expectedTotalSize = alignUp(minSizeRequested + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandQueueCommandStreamTest, getCommandStreamContainsMemoryForRequest) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(context.get(), pDevice, props);

    size_t requiredSize = 16384;
    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, getCommandStreamCanRecycle) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue commandQueue(context.get(), pDevice, props);

    auto &commandStreamInitial = commandQueue.getCS(1024);
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandQueue.getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandQueueCommandStreamTest, givenCommandStreamReceiverWithReusableAllocationsWhenAskedForCommandStreamReturnsAllocationFromReusablePool) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    size_t requiredSize = alignUp(100 + CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize, MemoryConstants::pageSize64k);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({requiredSize, GraphicsAllocation::AllocationType::COMMAND_BUFFER});
    auto &commandStreamReceiver = cmdQ.getCommandStreamReceiver();
    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
}
TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenItIsDestroyedThenCommandStreamIsPutOnTheReusabeList) {
    auto cmdQ = new CommandQueue(context.get(), pDevice, 0);
    const auto &commandStream = cmdQ->getCS(100);
    auto graphicsAllocation = commandStream.getGraphicsAllocation();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, CommandQueueWhenAskedForNewCommandStreamStoresOldHeapForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getCS(100);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    cmdQ.getCS(indirectHeap.getAvailableSpace() + 100);

    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_F(CommandQueueCommandStreamTest, givenCommandQueueWhenGetCSIsCalledThenCommandStreamAllocationTypeShouldBeSetToCommandBuffer) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    const auto &commandStream = cmdQ.getCS(100);
    auto commandStreamAllocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);

    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, commandStreamAllocation->getAllocationType());
}

HWTEST_F(CommandQueueCommandStreamTest, givenMultiDispatchInfoWithSingleKernelWithFlushAllocationsDisabledWhenEstimatingNodesCountEqualMultiDispatchInfoSize) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), pDevice, nullptr);
    cmdQ.multiEngineQueue = true;
    MockKernelWithInternals mockKernelWithInternals(*pDevice, context.get());

    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({mockKernelWithInternals.mockKernel}));

    size_t estimatedNodesCount = cmdQ.estimateTimestampPacketNodesCount(multiDispatchInfo);
    EXPECT_EQ(estimatedNodesCount, multiDispatchInfo.size());
}

HWTEST_F(CommandQueueCommandStreamTest, givenMultiDispatchInfoWithSingleKernelWithFlushAllocationsEnabledWhenEstimatingNodesCountEqualMultiDispatchInfoSizePlusOne) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    MockCommandQueueHw<FamilyType> cmdQ(context.get(), pDevice, nullptr);
    MockKernelWithInternals mockKernelWithInternals(*pDevice, context.get());

    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernelWithInternals.mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({mockKernelWithInternals.mockKernel}));

    size_t estimatedNodesCount = cmdQ.estimateTimestampPacketNodesCount(multiDispatchInfo);
    EXPECT_EQ(estimatedNodesCount, multiDispatchInfo.size() + 1);
}

struct CommandQueueIndirectHeapTest : public CommandQueueMemoryDevice,
                                      public ::testing::TestWithParam<IndirectHeap::Type> {
    void SetUp() override {
        CommandQueueMemoryDevice::SetUp();
        context.reset(new MockContext(pDevice));
    }

    void TearDown() override {
        context.reset();
        CommandQueueMemoryDevice::TearDown();
    }
    std::unique_ptr<MockContext> context;
};

TEST_P(CommandQueueIndirectHeapTest, IndirectHeapIsProvidedByDevice) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 8192);
    EXPECT_NE(nullptr, &indirectHeap);
}

TEST_P(CommandQueueIndirectHeapTest, givenIndirectObjectHeapWhenItIsQueriedForInternalAllocationThenTrueIsReturned) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 8192);
    if (this->GetParam() == IndirectHeap::INDIRECT_OBJECT) {
        EXPECT_TRUE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    } else {
        EXPECT_FALSE(indirectHeap.getGraphicsAllocation()->is32BitAllocation());
    }
}

HWTEST_P(CommandQueueIndirectHeapTest, IndirectHeapContainsAtLeast64KB) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), sizeof(uint32_t));
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        size_t expectedSshUse = cmdQ.getCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize - UnitTestHelper<FamilyType>::getDefaultSshUsage();
        EXPECT_EQ(expectedSshUse, indirectHeap.getAvailableSpace());
    } else {
        EXPECT_EQ(64 * KB, indirectHeap.getAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, getIndirectHeapContainsMemoryForRequest) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    size_t requiredSize = 16384;
    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_GE(indirectHeap.getMaxAvailableSpace(), requiredSize);
}

TEST_P(CommandQueueIndirectHeapTest, getIndirectHeapCanRecycle) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 10);
    size_t requiredSize = indirectHeapInitial.getMaxAvailableSpace() + 42;

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), requiredSize);
    ASSERT_NE(nullptr, &indirectHeap);
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        //no matter what SSH is always capped
        EXPECT_EQ(cmdQ.getCommandStreamReceiver().defaultSshSize - MemoryConstants::pageSize,
                  indirectHeap.getMaxAvailableSpace());
    } else {
        EXPECT_LE(requiredSize, indirectHeap.getMaxAvailableSpace());
    }
}

TEST_P(CommandQueueIndirectHeapTest, alignSizeToCacheLine) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);
    size_t minHeapSize = 64 * KB;

    auto &indirectHeapInitial = cmdQ.getIndirectHeap(this->GetParam(), 2 * minHeapSize + 1);

    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeapInitial.getAvailableSpace()));

    indirectHeapInitial.getSpace(indirectHeapInitial.getAvailableSpace()); // use whole space to force obtain reusable

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), minHeapSize + 1);

    ASSERT_NE(nullptr, &indirectHeap);
    EXPECT_TRUE(isAligned<MemoryConstants::cacheLineSize>(indirectHeap.getAvailableSpace()));
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandStreamReceiverWithReusableAllocationsWhenAskedForHeapAllocationReturnsAllocationFromReusablePool) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();

    auto allocationSize = defaultHeapSize * 2;

    GraphicsAllocation *allocation = nullptr;

    auto &commandStreamReceiver = cmdQ.getCommandStreamReceiver();
    auto allocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    if (this->GetParam() == IndirectHeap::INDIRECT_OBJECT) {
        allocationType = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }
    allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{allocationSize, allocationType});
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        allocation->setSize(commandStreamReceiver.defaultSshSize * 2);
    }

    commandStreamReceiver.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_FALSE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekContains(*allocation));

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);

    EXPECT_EQ(indirectHeap.getGraphicsAllocation(), allocation);

    // if we obtain heap from reusable pool, we need to keep the size of allocation
    // surface state heap is an exception, it is capped at (max_ssh_size_for_HW - page_size)
    if (this->GetParam() == IndirectHeap::SURFACE_STATE) {
        EXPECT_EQ(commandStreamReceiver.defaultSshSize - MemoryConstants::pageSize, indirectHeap.getMaxAvailableSpace());
    } else {
        EXPECT_EQ(allocationSize, indirectHeap.getMaxAvailableSpace());
    }

    EXPECT_TRUE(commandStreamReceiver.getAllocationsForReuse().peekIsEmpty());
}

TEST_P(CommandQueueIndirectHeapTest, CommandQueueWhenAskedForNewHeapStoresOldHeapForReuse) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_FALSE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekContains(*graphicsAllocation));
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocationWhenAskedForNewHeapReturnsAcquiresNewAllocationWithoutStoring) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pDevice, props);

    auto memoryManager = pDevice->getMemoryManager();
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getAvailableSpace();

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();

    csr.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    csr.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    // Request a larger heap than the first.
    cmdQ.getIndirectHeap(this->GetParam(), heapSize + 6000);

    EXPECT_NE(graphicsAllocation, indirectHeap.getGraphicsAllocation());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWithResourceCachingActiveWhenQueueISDestroyedThenIndirectHeapIsNotOnReuseList) {
    auto cmdQ = new CommandQueue(context.get(), pDevice, 0);
    cmdQ->getIndirectHeap(this->GetParam(), 100);
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    //now destroy command queue, heap should go to reusable list
    delete cmdQ;
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapAllocatedWhenIndirectHeapIsReleasedThenHeapAllocationAndHeapBufferIsSetToNullptr) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pDevice, props);

    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    const auto &indirectHeap = cmdQ.getIndirectHeap(this->GetParam(), 100);
    auto heapSize = indirectHeap.getMaxAvailableSpace();

    EXPECT_NE(0u, heapSize);

    auto graphicsAllocation = indirectHeap.getGraphicsAllocation();
    EXPECT_NE(nullptr, graphicsAllocation);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]->getGraphicsAllocation());

    EXPECT_EQ(nullptr, indirectHeap.getCpuBase());
    EXPECT_EQ(0u, indirectHeap.getMaxAvailableSpace());
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithoutHeapAllocatedWhenIndirectHeapIsReleasedThenIndirectHeapAllocationStaysNull) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pDevice, props);

    cmdQ.releaseIndirectHeap(this->GetParam());
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    EXPECT_EQ(nullptr, csr.indirectHeap[this->GetParam()]);
}

TEST_P(CommandQueueIndirectHeapTest, GivenCommandQueueWithHeapWhenGraphicAllocationIsNullThenNothingOnReuseList) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    MockCommandQueue cmdQ(context.get(), pDevice, props);

    auto &ih = cmdQ.getIndirectHeap(this->GetParam(), 0u);
    auto allocation = ih.getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);
    auto &csr = pDevice->getUltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>();

    csr.indirectHeap[this->GetParam()]->replaceGraphicsAllocation(nullptr);
    csr.indirectHeap[this->GetParam()]->replaceBuffer(nullptr, 0);

    cmdQ.releaseIndirectHeap(this->GetParam());

    auto memoryManager = pDevice->getMemoryManager();
    EXPECT_TRUE(pDevice->getDefaultEngine().commandStreamReceiver->getAllocationsForReuse().peekIsEmpty());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetIndirectHeapIsCalledThenIndirectHeapAllocationTypeShouldBeSetToInternalHeapForIohAndLinearStreamForOthers) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    auto heapType = this->GetParam();

    bool requireInternalHeap = IndirectHeap::INDIRECT_OBJECT == heapType;
    const auto &indirectHeap = cmdQ.getIndirectHeap(heapType, 100);
    auto indirectHeapAllocation = indirectHeap.getGraphicsAllocation();
    ASSERT_NE(nullptr, indirectHeapAllocation);
    auto expectedAllocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    if (requireInternalHeap) {
        expectedAllocationType = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    }
    EXPECT_EQ(expectedAllocationType, indirectHeapAllocation->getAllocationType());
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetHeapMemoryIsCalledThenHeapIsCreated) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    IndirectHeap *indirectHeap = nullptr;
    cmdQ.allocateHeapMemory(this->GetParam(), 100, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap->getGraphicsAllocation());

    pDevice->getMemoryManager()->freeGraphicsMemory(indirectHeap->getGraphicsAllocation());
    delete indirectHeap;
}

TEST_P(CommandQueueIndirectHeapTest, givenCommandQueueWhenGetHeapMemoryIsCalledWithAlreadyAllocatedHeapThenGraphicsAllocationIsCreated) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, 0, 0};
    CommandQueue cmdQ(context.get(), pDevice, props);

    IndirectHeap heap(nullptr, size_t{100});

    IndirectHeap *indirectHeap = &heap;
    cmdQ.allocateHeapMemory(this->GetParam(), 100, indirectHeap);
    EXPECT_EQ(&heap, indirectHeap);
    EXPECT_NE(nullptr, indirectHeap->getGraphicsAllocation());

    pDevice->getMemoryManager()->freeGraphicsMemory(indirectHeap->getGraphicsAllocation());
}

INSTANTIATE_TEST_CASE_P(
    Device,
    CommandQueueIndirectHeapTest,
    testing::Values(
        IndirectHeap::DYNAMIC_STATE,
        IndirectHeap::GENERAL_STATE,
        IndirectHeap::INDIRECT_OBJECT,
        IndirectHeap::SURFACE_STATE));

using CommandQueueTests = ::testing::Test;
HWTEST_F(CommandQueueTests, givenMultipleCommandQueuesWhenMarkerIsEmittedThenGraphicsAllocationIsReused) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
    MockContext context(device.get());
    std::unique_ptr<CommandQueue> commandQ(new CommandQueue(&context, device.get(), 0));
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = 0;
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);

    auto commandStreamGraphicsAllocation = commandQ->getCS(0).getGraphicsAllocation();
    commandQ.reset(new CommandQueue(&context, device.get(), 0));
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    commandQ->enqueueMarkerWithWaitList(0, nullptr, nullptr);
    auto commandStreamGraphicsAllocation2 = commandQ->getCS(0).getGraphicsAllocation();
    EXPECT_EQ(commandStreamGraphicsAllocation, commandStreamGraphicsAllocation2);
}

struct WaitForQueueCompletionTests : public ::testing::Test {
    template <typename Family>
    struct MyCmdQueue : public CommandQueueHw<Family> {
        MyCmdQueue(Context *context, Device *device) : CommandQueueHw<Family>(context, device, nullptr){};
        void waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
            requestedUseQuickKmdSleep = useQuickKmdSleep;
            waitUntilCompleteCounter++;
        }
        bool isQueueBlocked() override {
            return false;
        }
        bool requestedUseQuickKmdSleep = false;
        uint32_t waitUntilCompleteCounter = 0;
    };

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        context.reset(new MockContext(device.get()));
    }

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(WaitForQueueCompletionTests, givenBlockingCallAndUnblockedQueueWhenEnqueuedThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    uint32_t tmpPtr = 0;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    cmdQ->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 1, &tmpPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

HWTEST_F(WaitForQueueCompletionTests, givenBlockingCallAndBlockedQueueWhenEnqueuedThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    std::unique_ptr<Event> blockingEvent(new Event(cmdQ.get(), CL_COMMAND_NDRANGE_KERNEL, 0, 0));
    cl_event clBlockingEvent = blockingEvent.get();
    uint32_t tmpPtr = 0;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(context.get()));
    cmdQ->enqueueReadBuffer(buffer.get(), CL_TRUE, 0, 1, &tmpPtr, nullptr, 1, &clBlockingEvent, nullptr);
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

HWTEST_F(WaitForQueueCompletionTests, whenFinishIsCalledThenCallWaitWithoutQuickKmdSleepRequest) {
    std::unique_ptr<MyCmdQueue<FamilyType>> cmdQ(new MyCmdQueue<FamilyType>(context.get(), device.get()));
    cmdQ->finish(false);
    EXPECT_EQ(1u, cmdQ->waitUntilCompleteCounter);
    EXPECT_FALSE(cmdQ->requestedUseQuickKmdSleep);
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsWhenNoObjectsThenReturnSuccess) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    cl_uint numObjects = 0;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

class MockSharingHandler : public SharingHandler {
  public:
    void synchronizeObject(UpdateData &updateData) override {
        updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
    }
};

TEST(CommandQueue, givenEnqueuesForSharedObjectsWithImageUsingSharingHandlerThenReturnSuccess) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST(CommandQueue, givenEnqueuesForSharedObjectsWithImageUsingSharingHandlerWithEventThenReturnSuccess) {
    std::unique_ptr<MockDevice> mockDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockContext context;
    CommandQueue cmdQ(&context, mockDevice.get(), 0);
    MockSharingHandler *mockSharingHandler = new MockSharingHandler;

    auto image = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(&context));
    image->setSharingHandler(mockSharingHandler);

    cl_mem memObject = image.get();
    cl_uint numObjects = 1;
    cl_mem *memObjects = &memObject;

    Event *eventAcquire = new Event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_event clEventAquire = eventAcquire;
    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, &clEventAquire, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    ASSERT_NE(clEventAquire, nullptr);
    eventAcquire->release();

    Event *eventRelease = new Event(&cmdQ, CL_COMMAND_NDRANGE_KERNEL, 1, 5);
    cl_event clEventRelease = eventRelease;
    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, &clEventRelease, 0);
    EXPECT_EQ(result, CL_SUCCESS);
    ASSERT_NE(clEventRelease, nullptr);
    eventRelease->release();
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    cl_uint numObjects = 1;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    cl_mem memObject = nullptr;

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    memObject = buffer.get();

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueAcquireSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);
}

TEST(CommandQueue, givenEnqueueReleaseSharedObjectsWhenNoObjectsThenReturnSuccess) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    cl_uint numObjects = 0;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_SUCCESS);
}

TEST(CommandQueue, givenEnqueueReleaseSharedObjectsWhenIncorrectArgumentsThenReturnProperError) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    cl_uint numObjects = 1;
    cl_mem *memObjects = nullptr;

    cl_int result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    numObjects = 0;
    memObjects = (cl_mem *)1;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_VALUE);

    cl_mem memObject = nullptr;

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));
    memObject = buffer.get();

    numObjects = 1;
    memObjects = &memObject;

    result = cmdQ.enqueueReleaseSharedObjects(numObjects, memObjects, 0, nullptr, nullptr, 0);
    EXPECT_EQ(result, CL_INVALID_MEM_OBJECT);
}

TEST(CommandQueue, givenEnqueueAcquireSharedObjectsCallWhenAcquireFailsThenCorrectErrorIsReturned) {
    class MockSharingHandler : public SharingHandler {
        int validateUpdateData(UpdateData &data) override {
            return CL_INVALID_MEM_OBJECT;
        }
    };
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));

    MockSharingHandler *handler = new MockSharingHandler;
    buffer->setSharingHandler(handler);
    cl_mem memObject = buffer.get();
    auto retVal = cmdQ.enqueueAcquireSharedObjects(1, &memObject, 0, nullptr, nullptr, 0);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
    buffer->setSharingHandler(nullptr);
}

HWTEST_F(CommandQueueCommandStreamTest, givenDebugKernelWhenSetupDebugSurfaceIsCalledThenSurfaceStateIsCorrectlySet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    CommandQueue cmdQ(context.get(), pDevice, 0);

    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + kernel->getAllocatedKernelInfo()->patchInfo.pAllocateSystemThreadSurface->Offset);
    kernel->getAllocatedKernelInfo()->usesSsh = true;
    auto &commandStreamReceiver = cmdQ.getCommandStreamReceiver();

    cmdQ.setupDebugSurface(kernel.get());

    auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
    ASSERT_NE(nullptr, debugSurface);
    RENDER_SURFACE_STATE *surfaceState = (RENDER_SURFACE_STATE *)kernel->getSurfaceStateHeap();
    EXPECT_EQ(debugSurface->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

HWTEST_F(CommandQueueCommandStreamTest, givenCsrWithDebugSurfaceAllocatedWhenSetupDebugSurfaceIsCalledThenDebugSurfaceIsReused) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    CommandQueue cmdQ(context.get(), pDevice, 0);

    kernel->setSshLocal(nullptr, sizeof(RENDER_SURFACE_STATE) + kernel->getAllocatedKernelInfo()->patchInfo.pAllocateSystemThreadSurface->Offset);
    kernel->getAllocatedKernelInfo()->usesSsh = true;
    auto &commandStreamReceiver = cmdQ.getCommandStreamReceiver();
    commandStreamReceiver.allocateDebugSurface(SipKernel::maxDbgSurfaceSize);
    auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
    ASSERT_NE(nullptr, debugSurface);

    cmdQ.setupDebugSurface(kernel.get());

    EXPECT_EQ(debugSurface, commandStreamReceiver.getDebugSurfaceAllocation());
    RENDER_SURFACE_STATE *surfaceState = (RENDER_SURFACE_STATE *)kernel->getSurfaceStateHeap();
    EXPECT_EQ(debugSurface->getGpuAddress(), surfaceState->getSurfaceBaseAddress());
}

struct MockTimestampPacketContainer : TimestampPacketContainer {
    MockTimestampPacketContainer(Context &context) : context(context) {
    }
    ~MockTimestampPacketContainer() override {
        EXPECT_EQ(1, context.getRefInternalCount());
    }
    Context &context;
};

TEST(CommandQueueDestructorTest, whenCommandQueueIsDestroyedThenDestroysTimestampPacketContainerBeforeReleasingContext) {
    auto context = new MockContext;
    EXPECT_EQ(1, context->getRefInternalCount());
    MockCommandQueue queue(context, context->getDevice(0), nullptr);
    queue.timestampPacketContainer.reset(new MockTimestampPacketContainer(*context));
    EXPECT_EQ(2, context->getRefInternalCount());
    context->release();
    EXPECT_EQ(1, context->getRefInternalCount());
}

TEST(CommandQueuePropertiesTests, whenDefaultCommandQueueIsCreatedThenItIsNotMultiEngineQueue) {
    MockCommandQueue queue;
    EXPECT_FALSE(queue.multiEngineQueue);
    EXPECT_FALSE(queue.isMultiEngineQueue());
    queue.multiEngineQueue = true;
    EXPECT_TRUE(queue.isMultiEngineQueue());
}

TEST(CommandQueuePropertiesTests, whenGetEngineIsCalledThenQueueEngineIsReturned) {
    MockCommandQueue queue;
    EngineControl engineControl;
    queue.engine = &engineControl;
    EXPECT_EQ(queue.engine, &queue.getEngine());
}
TEST(CommandQueue, GivenCommandQueueWhenEnqueueResourceBarrierCalledThenSuccessReturned) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    cl_int result = cmdQ.enqueueResourceBarrier(
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, result);
}
TEST(CommandQueue, GivenCommandQueueWhenCheckingIfIsCacheFlushCommandCalledThenFalseReturned) {
    MockContext context;
    CommandQueue cmdQ(&context, nullptr, 0);

    bool isCommandCacheFlush = cmdQ.isCacheFlushCommand(0u);
    EXPECT_FALSE(isCommandCacheFlush);
}
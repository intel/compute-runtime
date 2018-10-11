/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_program.h"

#include "gmock/gmock.h"

using namespace OCLRT;

struct CommandStreamReceiverTest : public DeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();

        commandStreamReceiver = &pDevice->getCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    CommandStreamReceiver *commandStreamReceiver;
};

HWTEST_F(CommandStreamReceiverTest, testCtor) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
}

HWTEST_F(CommandStreamReceiverTest, testInitProgrammingFlags) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.initProgrammingFlags();
    EXPECT_FALSE(csr.isPreambleProgrammed());
    EXPECT_FALSE(csr.isGSBAFor32BitProgrammed());
    EXPECT_TRUE(csr.isMediaVfeStateDirty());
    EXPECT_FALSE(csr.isLastVmeSubslicesConfig());
    EXPECT_EQ(0u, csr.getLastSentL3Config());
    EXPECT_EQ(-1, csr.getLastSentCoherencyRequest());
    EXPECT_EQ(-1, csr.getLastMediaSamplerConfig());
    EXPECT_EQ(PreemptionMode::Initial, csr.getLastPreemptionMode());
    EXPECT_EQ(0u, csr.getLatestSentStatelessMocsConfig());
}

TEST_F(CommandStreamReceiverTest, makeResident_setsBufferResidencyFlag) {
    MockContext context;
    float srcMemory[] = {1.0f};

    auto retVal = CL_INVALID_VALUE;
    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, buffer);
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident(0u));

    commandStreamReceiver->makeResident(*buffer->getGraphicsAllocation());

    EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident(0u));

    delete buffer;
}

TEST_F(CommandStreamReceiverTest, commandStreamReceiverFromDeviceHasATagValue) {
    EXPECT_NE(nullptr, const_cast<uint32_t *>(commandStreamReceiver->getTagAddress()));
}

TEST_F(CommandStreamReceiverTest, GetCommandStreamReturnsValidObject) {
    auto &cs = commandStreamReceiver->getCS();
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandStreamReceiverTest, getCommandStreamContainsMemoryForRequest) {
    size_t requiredSize = 16384;
    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, getCsReturnsCsWithCsOverfetchSizeIncludedInGraphicsAllocation) {
    size_t sizeRequested = 560;
    const auto &commandStream = commandStreamReceiver->getCS(sizeRequested);
    ASSERT_NE(nullptr, &commandStream);
    auto *allocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    size_t expectedTotalSize = alignUp(sizeRequested + MemoryConstants::cacheLineSize, MemoryConstants::pageSize) + CSRequirements::csOverfetchSize;

    EXPECT_LT(commandStream.getAvailableSpace(), expectedTotalSize);
    EXPECT_LE(commandStream.getAvailableSpace(), expectedTotalSize - CSRequirements::csOverfetchSize);
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandStreamReceiverTest, getCommandStreamCanRecycle) {
    auto &commandStreamInitial = commandStreamReceiver->getCS();
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenGetCSIsCalledThenCommandStreamAllocationTypeShouldBeSetToLinearStream) {
    const auto &commandStream = commandStreamReceiver->getCS();
    auto commandStreamAllocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);

    EXPECT_EQ(GraphicsAllocation::AllocationType::LINEAR_STREAM, commandStreamAllocation->getAllocationType());
}

TEST_F(CommandStreamReceiverTest, createAllocationAndHandleResidency) {
    void *host_ptr = (void *)0x1212341;
    auto size = 17262u;

    GraphicsAllocation *graphicsAllocation = commandStreamReceiver->createAllocationAndHandleResidency(host_ptr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(host_ptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
}

TEST_F(CommandStreamReceiverTest, givenCommandStreamerWhenAllocationIsNotAddedToListThenCallerMustFreeAllocation) {
    void *hostPtr = reinterpret_cast<void *>(0x1212341);
    auto size = 17262u;

    GraphicsAllocation *graphicsAllocation = commandStreamReceiver->createAllocationAndHandleResidency(hostPtr, size, false);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(hostPtr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());

    commandStreamReceiver->getMemoryManager()->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamerWhenPtrAndSizeMeetL3CriteriaThenCsrEnableL3) {
    void *hostPtr = reinterpret_cast<void *>(0xF000);
    auto size = 0x2000u;

    GraphicsAllocation *graphicsAllocation = commandStreamReceiver->createAllocationAndHandleResidency(hostPtr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(csr.disableL3Cache);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamerWhenPtrAndSizeDoNotMeetL3CriteriaThenCsrDisableL3) {
    void *hostPtr = reinterpret_cast<void *>(0xF001);
    auto size = 0x2001u;

    GraphicsAllocation *graphicsAllocation = commandStreamReceiver->createAllocationAndHandleResidency(hostPtr, size);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(csr.disableL3Cache);
}

TEST_F(CommandStreamReceiverTest, memoryManagerHasAccessToCSR) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    EXPECT_EQ(commandStreamReceiver, memoryManager->getCommandStreamReceiver(0));
}

HWTEST_F(CommandStreamReceiverTest, storedAllocationsHaveCSRtaskCount) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memoryManager = csr.getMemoryManager();
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);

    csr.taskCount = 2u;

    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_EQ(csr.peekTaskCount(), allocation->taskCount);
}

HWTEST_F(CommandStreamReceiverTest, dontReuseSurfaceIfStillInUse) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memoryManager = csr.getMemoryManager();
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);

    csr.taskCount = 2u;

    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    auto *hwTag = csr.getTagAddress();

    *hwTag = 1;

    auto newAllocation = memoryManager->obtainReusableAllocation(1, false);

    EXPECT_EQ(nullptr, newAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCheckedForInitialStatusOfStatelessMocsIndexThenUnknownMocsIsReturend) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CacheSettings::unknownMocs, csr.latestSentStatelessMocsConfig);
    EXPECT_FALSE(csr.disableL3Cache);
}

TEST_F(CommandStreamReceiverTest, makeResidentPushesAllocationToMemoryManagerResidencyList) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(0x1000);

    ASSERT_NE(nullptr, graphicsAllocation);

    commandStreamReceiver->makeResident(*graphicsAllocation);

    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();

    ASSERT_EQ(1u, residencyAllocations.size());
    EXPECT_EQ(graphicsAllocation, residencyAllocations[0]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, makeResidentWithoutParametersDoesNothing) {
    commandStreamReceiver->processResidency(commandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();
    EXPECT_EQ(0u, residencyAllocations.size());
}

TEST_F(CommandStreamReceiverTest, givenForced32BitAddressingWhenDebugSurfaceIsAllocatedThenRegularAllocationIsReturned) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    memoryManager->setForce32BitAllocations(true);
    auto allocation = commandStreamReceiver->allocateDebugSurface(1024);
    EXPECT_FALSE(allocation->is32BitAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetIndirectHeapIsCalledThenHeapIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10u);
    EXPECT_NE(nullptr, heap.getGraphicsAllocation());
    EXPECT_NE(nullptr, csr.indirectHeap[IndirectHeap::DYNAMIC_STATE]);
    EXPECT_EQ(&heap, csr.indirectHeap[IndirectHeap::DYNAMIC_STATE]);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenReleaseIndirectHeapIsCalledThenHeapAllocationIsNull) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10u);
    csr.releaseIndirectHeap(IndirectHeap::DYNAMIC_STATE);
    EXPECT_EQ(nullptr, heap.getGraphicsAllocation());
    EXPECT_EQ(0u, heap.getMaxAvailableSpace());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenAllocateHeapMemoryIsCalledThenHeapMemoryIsAllocated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *dsh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    EXPECT_NE(nullptr, dsh);
    ASSERT_NE(nullptr, dsh->getGraphicsAllocation());
    csr.getMemoryManager()->freeGraphicsMemory(dsh->getGraphicsAllocation());
    delete dsh;
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWithoutTagAllocationWhenGetTagAllocationIsCalledThenNullptrIsReturned) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    EXPECT_EQ(nullptr, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenDebugVariableEnabledWhenCreatingCsrThenEnableTimestampPacketWriteMode) {
    DebugManagerStateRestore restore;

    DebugManager.flags.EnableTimestampPacket.set(true);
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr1(executionEnvironment);
    EXPECT_TRUE(csr1.peekTimestampPacketWriteEnabled());

    DebugManager.flags.EnableTimestampPacket.set(false);
    MockCommandStreamReceiver csr2(executionEnvironment);
    EXPECT_FALSE(csr2.peekTimestampPacketWriteEnabled());
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWithTagAllocationSetWhenGetTagAllocationIsCalledThenCorrectAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    GraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    csr.setTagAllocation(&allocation);
    EXPECT_EQ(&allocation, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenItIsDestroyedThenItDestroysTagAllocation) {
    struct MockGraphicsAllocation : public GraphicsAllocation {
        using GraphicsAllocation::GraphicsAllocation;
        ~MockGraphicsAllocation() override { *destructorCalled = true; }
        bool *destructorCalled = nullptr;
    };

    bool destructorCalled = false;

    auto mockGraphicsAllocation = new MockGraphicsAllocation(nullptr, 1u);
    mockGraphicsAllocation->destructorCalled = &destructorCalled;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.commandStreamReceivers.push_back(std::make_unique<MockCommandStreamReceiver>(executionEnvironment));
    auto csr = executionEnvironment.commandStreamReceivers[0].get();
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    csr->setTagAllocation(mockGraphicsAllocation);
    EXPECT_FALSE(destructorCalled);
    executionEnvironment.commandStreamReceivers[0].reset(nullptr);
    EXPECT_TRUE(destructorCalled);
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenInitializeTagAllocationIsCalledThenTagAllocationIsBeingAllocated) {
    ExecutionEnvironment executionEnvironment;
    auto csr = new MockCommandStreamReceiver(executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() == nullptr);
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() != nullptr);
    EXPECT_EQ(*csr->getTagAddress(), initialHardwareTag);
}

TEST(CommandStreamReceiverSimpleTest, givenNullHardwareDebugModeWhenInitializeTagAllocationIsCalledThenTagAllocationIsBeingAllocatedAndinitialValueIsMinusOne) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableNullHardware.set(true);
    ExecutionEnvironment executionEnvironment;
    auto csr = new MockCommandStreamReceiver(executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() == nullptr);
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() != nullptr);
    EXPECT_EQ(*csr->getTagAddress(), static_cast<uint32_t>(-1));
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWhenWaitBeforeMakingNonResidentWhenRequiredIsCalledWithBlockingFlagSetThenItReturnsImmediately) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    uint32_t tag = 0;
    GraphicsAllocation allocation(&tag, sizeof(tag));
    csr.latestFlushedTaskCount = 3;
    csr.setTagAllocation(&allocation);
    csr.waitBeforeMakingNonResidentWhenRequired();

    EXPECT_EQ(0u, tag);
}

TEST(CommandStreamReceiverMultiContextTests, givenMultipleCsrsWhenSameResourcesAreUsedThenResidencyIsProperlyHandled) {
    auto executionEnvironment = new ExecutionEnvironment;

    std::unique_ptr<Device> device0(Device::create<Device>(nullptr, executionEnvironment, 0u));
    std::unique_ptr<Device> device1(Device::create<Device>(nullptr, executionEnvironment, 1u));

    auto &commandStreamReceiver0 = device0->getCommandStreamReceiver();
    auto &commandStreamReceiver1 = device1->getCommandStreamReceiver();

    auto graphicsAllocation = executionEnvironment->memoryManager->allocateGraphicsMemory(4096u);

    commandStreamReceiver0.makeResident(*graphicsAllocation);
    commandStreamReceiver1.makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getResidencyAllocations().size());

    EXPECT_EQ(1, graphicsAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(1, graphicsAllocation->residencyTaskCount[1u]);

    commandStreamReceiver0.makeNonResident(*graphicsAllocation);
    commandStreamReceiver1.makeNonResident(*graphicsAllocation);

    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount[0u]);
    EXPECT_EQ(ObjectNotResident, graphicsAllocation->residencyTaskCount[1u]);

    EXPECT_EQ(1u, commandStreamReceiver0.getEvictionAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getEvictionAllocations().size());

    executionEnvironment->memoryManager->freeGraphicsMemory(graphicsAllocation);
}

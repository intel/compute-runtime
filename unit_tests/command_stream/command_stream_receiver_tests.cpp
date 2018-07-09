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

#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_program.h"
#include "test.h"
#include "runtime/helpers/cache_policy.h"

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
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident());

    commandStreamReceiver->makeResident(*buffer->getGraphicsAllocation());

    EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident());

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
    EXPECT_EQ(commandStreamReceiver, memoryManager->csr);
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

    auto &residencyAllocations = memoryManager->getResidencyAllocations();

    ASSERT_EQ(1u, residencyAllocations.size());
    EXPECT_EQ(graphicsAllocation, residencyAllocations[0]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, makeResidentWithoutParametersDoesNothing) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    commandStreamReceiver->processResidency(nullptr);
    auto &residencyAllocations = memoryManager->getResidencyAllocations();
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
    MockCommandStreamReceiver csr;
    EXPECT_EQ(nullptr, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWithTagAllocationSetWhenGetTagAllocationIsCalledThenCorrectAllocationIsReturned) {
    MockCommandStreamReceiver csr;
    GraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    csr.setTagAllocation(&allocation);
    EXPECT_EQ(&allocation, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWhenWaitBeforeMakingNonResidentWhenRequiredIsCalledWithBlockingFlagSetThenItReturnsImmediately) {
    MockCommandStreamReceiver csr;
    uint32_t tag = 0;
    GraphicsAllocation allocation(&tag, sizeof(tag));
    csr.latestFlushedTaskCount = 3;
    csr.setTagAllocation(&allocation);
    csr.waitBeforeMakingNonResidentWhenRequired(true);

    EXPECT_EQ(0u, tag);
}
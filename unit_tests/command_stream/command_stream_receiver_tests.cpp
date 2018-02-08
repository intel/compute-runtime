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
#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_csr.h"
#include "test.h"
#include "runtime/helpers/cache_policy.h"

#include "gmock/gmock-matchers.h"

using namespace OCLRT;

struct CommandStreamReceiverTest : public DeviceFixture,
                                   public MemoryManagementFixture,
                                   public ::testing::Test {
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        DeviceFixture::SetUp();

        commandStreamReceiver = &pDevice->getCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
        MemoryManagementFixture::TearDown();
    }

    CommandStreamReceiver *commandStreamReceiver;
};

HWTEST_F(CommandStreamReceiverTest, testCtor) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
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

    auto newAllocation = memoryManager->obtainReusableAllocation(1);

    EXPECT_EQ(nullptr, newAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCheckedForInitialStatusOfStatelessMocsIndexThenUnknownMocsIsReturend) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CacheSettings::unknownMocs, csr.latestSentStatelessMocsConfig);
    EXPECT_FALSE(csr.disableL3Cache);
}

TEST_F(CommandStreamReceiverTest, makeResidentPushesAllocationToMemoryManagerResidencyList) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(0x1000, 0x34000);

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

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CommandStreamReceiver::DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

TEST(CommandStreamReceiver, cmdStreamReceiverReservedBlockInInstructionHeapIsBasedOnPreemptionHelper) {
    char pattern[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 39, 41};

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(nullptr));
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);

    {
        MockBuiltins mockBuiltins;
        mockBuiltins.overrideGlobalBuiltins();
        {
            auto sipOverride = std::unique_ptr<OCLRT::SipKernel>(new OCLRT::SipKernel(OCLRT::SipKernelType::Csr,
                                                                                      pattern, sizeof(pattern)));
            mockBuiltins.overrideSipKernel(std::move(sipOverride));
        }

        size_t reservedSize = mockDevice->getCommandStreamReceiver().getInstructionHeapCmdStreamReceiverReservedSize();
        size_t expectedSize = OCLRT::PreemptionHelper::getInstructionHeapSipKernelReservedSize(*mockDevice);
        EXPECT_NE(0U, expectedSize);
        EXPECT_EQ(expectedSize, reservedSize);
        ASSERT_LE(expectedSize, reservedSize);

        StackVec<char, 4096> cmdStreamIhBuffer;
        cmdStreamIhBuffer.resize(reservedSize);
        LinearStream cmdStreamReceiverInstrucionHeap{cmdStreamIhBuffer.begin(), cmdStreamIhBuffer.size()};
        mockDevice->getCommandStreamReceiver().initializeInstructionHeapCmdStreamReceiverReservedBlock(cmdStreamReceiverInstrucionHeap);

        StackVec<char, 4096> preemptionHelperIhBuffer;
        preemptionHelperIhBuffer.resize(expectedSize);
        LinearStream preemptionHelperInstrucionHeap{preemptionHelperIhBuffer.begin(), preemptionHelperIhBuffer.size()};
        PreemptionHelper::initializeInstructionHeapSipKernelReservedBlock(preemptionHelperInstrucionHeap, *mockDevice);

        cmdStreamIhBuffer.resize(expectedSize);
        EXPECT_THAT(preemptionHelperIhBuffer, testing::ContainerEq(cmdStreamIhBuffer));
    }
}

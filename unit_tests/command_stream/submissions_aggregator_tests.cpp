/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "runtime/command_stream/submissions_aggregator.h"
#include "runtime/helpers/flush_stamp.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_command_queue.h"

using namespace OCLRT;

struct MockSubmissionAggregator : public SubmissionAggregator {
    CommandBufferList &peekCommandBuffersList() {
        return this->cmdBuffers;
    }
};

TEST(SubmissionsAggregator, givenDefaultSubmissionsAggregatorWhenItIsCreatedThenCreationIsSuccesful) {
    MockSubmissionAggregator submissionsAggregator;
    EXPECT_TRUE(submissionsAggregator.peekCommandBuffersList().peekIsEmpty());
}

TEST(SubmissionsAggregator, givenCommandBufferWhenItIsPassedToSubmissionsAggregatorThenItIsRecorded) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    EXPECT_FALSE(submissionsAggregator.peekCommandBuffersList().peekIsEmpty());
    EXPECT_EQ(cmdBuffer, submissionsAggregator.peekCommandBuffersList().peekHead());
    EXPECT_EQ(cmdBuffer, submissionsAggregator.peekCommandBuffersList().peekTail());
    EXPECT_EQ(cmdBuffer->surfaces.size(), 0u);
    //idlist holds the ownership
}

TEST(SubmissionsAggregator, givenTwoCommandBuffersWhenMergeResourcesIsCalledThenDuplicatesAreEliminated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc2(nullptr, 2);
    GraphicsAllocation alloc3(nullptr, 3);
    GraphicsAllocation alloc4(nullptr, 4);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc6(nullptr, 6);

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = -1;
    ResourcePackage resourcePackage;

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    EXPECT_EQ(0u, totalUsedSize);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    EXPECT_EQ(15u, totalUsedSize);
    totalUsedSize = 0;
    resourcePackage.clear();

    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    EXPECT_EQ(cmdBuffer, submissionsAggregator.peekCommandBuffersList().peekHead());
    EXPECT_EQ(cmdBuffer2, submissionsAggregator.peekCommandBuffersList().peekTail());
    EXPECT_NE(submissionsAggregator.peekCommandBuffersList().peekHead(), submissionsAggregator.peekCommandBuffersList().peekTail());

    EXPECT_EQ(5u, cmdBuffer->surfaces.size());
    EXPECT_EQ(4u, cmdBuffer2->surfaces.size());

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //command buffer 2 is aggregated to command buffer 1
    auto primaryBatchInstepctionId = submissionsAggregator.peekCommandBuffersList().peekHead()->inspectionId;
    EXPECT_EQ(primaryBatchInstepctionId, submissionsAggregator.peekCommandBuffersList().peekHead()->next->inspectionId);
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(6u, resourcePackage.size());
    EXPECT_EQ(21u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenSubmissionAggregatorWhenThreeCommandBuffersAreSubmittedThenTheyAreAggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer3 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc2(nullptr, 2);
    GraphicsAllocation alloc3(nullptr, 3);
    GraphicsAllocation alloc4(nullptr, 4);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc6(nullptr, 6);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    cmdBuffer3->surfaces.push_back(&alloc7);
    cmdBuffer3->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = -1;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    submissionsAggregator.recordCommandBuffer(cmdBuffer3);

    EXPECT_EQ(cmdBuffer, submissionsAggregator.peekCommandBuffersList().peekHead());
    EXPECT_EQ(cmdBuffer3, submissionsAggregator.peekCommandBuffersList().peekTail());
    EXPECT_EQ(cmdBuffer3->prev, cmdBuffer2);
    EXPECT_EQ(cmdBuffer2->next, cmdBuffer3);
    EXPECT_EQ(cmdBuffer->next, cmdBuffer2);
    EXPECT_EQ(cmdBuffer2->prev, cmdBuffer);

    EXPECT_NE(submissionsAggregator.peekCommandBuffersList().peekHead(), submissionsAggregator.peekCommandBuffersList().peekTail());

    EXPECT_EQ(5u, cmdBuffer->surfaces.size());
    EXPECT_EQ(4u, cmdBuffer2->surfaces.size());
    EXPECT_EQ(2u, cmdBuffer3->surfaces.size());

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //command buffer 3 and 2 is aggregated to command buffer 1
    auto primaryBatchInstepctionId = submissionsAggregator.peekCommandBuffersList().peekHead()->inspectionId;

    EXPECT_EQ(primaryBatchInstepctionId, submissionsAggregator.peekCommandBuffersList().peekHead()->next->inspectionId);
    EXPECT_EQ(primaryBatchInstepctionId, submissionsAggregator.peekCommandBuffersList().peekHead()->next->next->inspectionId);

    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(7u, resourcePackage.size());
    EXPECT_EQ(28u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenMultipleCommandBuffersWhenTheyAreAggreagateWithCertainMemoryLimitThenOnlyThatFitAreAggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer3 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc2(nullptr, 2);
    GraphicsAllocation alloc3(nullptr, 3);
    GraphicsAllocation alloc4(nullptr, 4);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc6(nullptr, 6);
    GraphicsAllocation alloc7(nullptr, 7);

    //14 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    //12 bytes total , only 7 new
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    //12 bytes total, only 7 new
    cmdBuffer3->surfaces.push_back(&alloc7);
    cmdBuffer3->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 22;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    submissionsAggregator.recordCommandBuffer(cmdBuffer3);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //command buffer 2 is aggregated to command buffer 1, comand buffer 3 becomes command buffer 2
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekTail(), cmdBuffer3);
    EXPECT_EQ(cmdBuffer->next, cmdBuffer2);
    EXPECT_EQ(cmdBuffer3->prev, cmdBuffer2);
    EXPECT_EQ(cmdBuffer2->inspectionId, cmdBuffer->inspectionId);
    EXPECT_NE(cmdBuffer3->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(0u, cmdBuffer3->inspectionId);

    EXPECT_EQ(6u, resourcePackage.size());
    EXPECT_EQ(21u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenMultipleCommandBuffersWhenAggregateIsCalledMultipleTimesThenFurtherInspectionAreHandledCorrectly) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer3 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc2(nullptr, 2);
    GraphicsAllocation alloc3(nullptr, 3);
    GraphicsAllocation alloc4(nullptr, 4);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc6(nullptr, 6);
    GraphicsAllocation alloc7(nullptr, 7);

    //14 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    //12 bytes total , only 7 new
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    //12 bytes total, only 7 new
    cmdBuffer3->surfaces.push_back(&alloc7);
    cmdBuffer3->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 14;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    submissionsAggregator.recordCommandBuffer(cmdBuffer3);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //command buffers not aggregated due to too low limit
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(cmdBuffer->next, cmdBuffer2);
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekTail(), cmdBuffer3);

    //budget is now larger we can fit everything
    totalMemoryBudget = 28;
    resourcePackage.clear();
    totalUsedSize = 0;

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);
    //all cmd buffers are merged to 1
    EXPECT_EQ(cmdBuffer3->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);

    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekTail(), cmdBuffer3);
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(totalMemoryBudget, totalUsedSize);
    EXPECT_EQ(7u, resourcePackage.size());
}

TEST(SubmissionsAggregator, givenMultipleCommandBuffersWithDifferentGraphicsAllocationsWhenAggregateIsCalledThenResourcePackContainSecondBatchBuffer) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc2(nullptr, 2);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc7(nullptr, 7);

    //5 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);

    //10 bytes total
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->batchBuffer.commandBufferAllocation = &alloc7;

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    EXPECT_EQ(4u, resourcePackage.size());
    EXPECT_EQ(15u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenTwoCommandBufferWhereSecondContainsFirstOnResourceListWhenItIsAggregatedThenResourcePackDoesntContainPrimaryBatch) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation cmdBufferAllocation1(nullptr, 1);
    GraphicsAllocation cmdBufferAllocation2(nullptr, 2);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;
    cmdBuffer2->batchBuffer.commandBufferAllocation = &cmdBufferAllocation2;

    //cmdBuffer2 has commandBufferAllocation on the surface list
    cmdBuffer2->surfaces.push_back(&cmdBufferAllocation1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    cmdBuffer->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //resource pack shuold have 3 surfaces
    EXPECT_EQ(3u, resourcePackage.size());
    EXPECT_EQ(14u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenTwoCommandBufferWhereSecondContainsTheFirstCommandBufferGraphicsAllocaitonWhenItIsAggregatedThenResourcePackDoesntContainPrimaryBatch) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation cmdBufferAllocation1(nullptr, 1);
    GraphicsAllocation alloc5(nullptr, 5);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;
    cmdBuffer2->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;

    //cmdBuffer2 has commandBufferAllocation on the surface list
    cmdBuffer2->surfaces.push_back(&alloc7);
    cmdBuffer->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);

    //resource pack shuold have 3 surfaces
    EXPECT_EQ(2u, resourcePackage.size());
    EXPECT_EQ(12u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenCommandBuffersRequiringDifferentCoherencySettingWhenAggregateIsCalledThenTheyAreNotAgggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.requiresCoherency = true;
    cmdBuffer2->batchBuffer.requiresCoherency = false;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);
    EXPECT_EQ(1u, totalUsedSize);
    EXPECT_EQ(1u, resourcePackage.size());
    EXPECT_NE(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(1u, cmdBuffer->inspectionId);
}

TEST(SubmissionsAggregator, givenCommandBuffersRequiringDifferentThrottleSettingWhenAggregateIsCalledThenTheyAreNotAgggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.throttle = QueueThrottle::LOW;
    cmdBuffer2->batchBuffer.throttle = QueueThrottle::MEDIUM;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);
    EXPECT_EQ(1u, totalUsedSize);
    EXPECT_EQ(1u, resourcePackage.size());
    EXPECT_NE(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(1u, cmdBuffer->inspectionId);
}

TEST(SubmissionsAggregator, givenCommandBuffersRequiringDifferentPrioritySettingWhenAggregateIsCalledThenTheyAreNotAgggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    GraphicsAllocation alloc1(nullptr, 1);
    GraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.low_priority = true;
    cmdBuffer2->batchBuffer.low_priority = false;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget);
    EXPECT_EQ(1u, totalUsedSize);
    EXPECT_EQ(1u, resourcePackage.size());
    EXPECT_NE(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(1u, cmdBuffer->inspectionId);
}

TEST(SubmissionsAggregator, dontAllocateFlushStamp) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer cmdBuffer(*device);
    EXPECT_EQ(nullptr, cmdBuffer.flushStamp->getStampReference());
}

struct SubmissionsAggregatorTests : public ::testing::Test {
    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        context.reset(new MockContext(device.get()));
    }

    template <typename T>
    void overrideCsr(T *newCsr) {
        device->resetCommandStreamReceiver(newCsr);
        newCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    }

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(SubmissionsAggregatorTests, givenMultipleQueuesWhenCmdBuffersAreRecordedThenAssignFlushStampObjFromCmdQueue) {
    MockKernelWithInternals kernel(*device.get());
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *device->executionEnvironment);
    size_t GWS = 1;

    overrideCsr(mockCsr);

    auto expectRefCounts = [&](int32_t cmdQRef1, int32_t cmdQRef2) {
        EXPECT_EQ(cmdQRef1, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());
        EXPECT_EQ(cmdQRef2, cmdQ2.flushStamp->getStampReference()->getRefInternalCount());
    };

    expectRefCounts(1, 1);
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    expectRefCounts(2, 1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);
    expectRefCounts(2, 2);

    {
        auto cmdBuffer = mockCsr->peekSubmissionAggregator()->peekCmdBufferList().removeFrontOne();
        EXPECT_EQ(cmdQ1.flushStamp->getStampReference(), cmdBuffer->flushStamp->getStampReference());
    }
    expectRefCounts(1, 2);
    {
        auto cmdBuffer = mockCsr->peekSubmissionAggregator()->peekCmdBufferList().removeFrontOne();
        EXPECT_EQ(cmdQ2.flushStamp->getStampReference(), cmdBuffer->flushStamp->getStampReference());
    }

    expectRefCounts(1, 1);
}

HWTEST_F(SubmissionsAggregatorTests, givenCmdQueueWhenCmdBufferWithEventIsRecordedThenAssignFlushStampObjForEveryone) {
    MockKernelWithInternals kernel(*device.get());
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *device->executionEnvironment);
    size_t GWS = 1;

    overrideCsr(mockCsr);

    cl_event event1;
    EXPECT_EQ(1, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, &event1);
    EXPECT_EQ(3, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());

    EXPECT_EQ(castToObject<Event>(event1)->flushStamp->getStampReference(), cmdQ1.flushStamp->getStampReference());

    {
        auto cmdBuffer = mockCsr->peekSubmissionAggregator()->peekCmdBufferList().removeFrontOne();
        EXPECT_EQ(cmdQ1.flushStamp->getStampReference(), cmdBuffer->flushStamp->getStampReference());
    }
    EXPECT_EQ(2, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());

    castToObject<Event>(event1)->release();
    EXPECT_EQ(1, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());
}

HWTEST_F(SubmissionsAggregatorTests, givenMultipleCmdBuffersWhenFlushThenUpdateAllRelatedFlushStamps) {
    MockKernelWithInternals kernel(*device.get());
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *device->executionEnvironment);
    size_t GWS = 1;

    overrideCsr(mockCsr);
    mockCsr->taskCount = 5;
    mockCsr->flushStamp->setStamp(5);

    cl_event event1, event2;
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, &event1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, &event2);

    mockCsr->flushBatchedSubmissions();

    auto expectedFlushStamp = mockCsr->flushStamp->peekStamp();
    EXPECT_EQ(expectedFlushStamp, cmdQ1.flushStamp->peekStamp());
    EXPECT_EQ(expectedFlushStamp, cmdQ2.flushStamp->peekStamp());
    EXPECT_EQ(expectedFlushStamp, castToObject<Event>(event1)->flushStamp->peekStamp());
    EXPECT_EQ(expectedFlushStamp, castToObject<Event>(event2)->flushStamp->peekStamp());

    castToObject<Event>(event1)->release();
    castToObject<Event>(event2)->release();
}

HWTEST_F(SubmissionsAggregatorTests, givenMultipleCmdBuffersWhenNotAggregatedDuringFlushThenUpdateAllRelatedFlushStamps) {
    MockKernelWithInternals kernel(*device.get());
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0);
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *device->executionEnvironment);
    size_t GWS = 1;

    overrideCsr(mockCsr);
    mockCsr->taskCount = 5;
    mockCsr->flushStamp->setStamp(5);

    cl_event event1, event2;
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, &event1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, &event2);

    // dont aggregate
    mockCsr->peekSubmissionAggregator()->peekCmdBufferList().peekHead()->batchBuffer.low_priority = true;
    mockCsr->peekSubmissionAggregator()->peekCmdBufferList().peekTail()->batchBuffer.low_priority = false;

    mockCsr->flushBatchedSubmissions();

    EXPECT_EQ(6u, cmdQ1.flushStamp->peekStamp());
    EXPECT_EQ(6u, castToObject<Event>(event1)->flushStamp->peekStamp());
    EXPECT_EQ(7u, cmdQ2.flushStamp->peekStamp());
    EXPECT_EQ(7u, castToObject<Event>(event2)->flushStamp->peekStamp());

    castToObject<Event>(event1)->release();
    castToObject<Event>(event2)->release();
}

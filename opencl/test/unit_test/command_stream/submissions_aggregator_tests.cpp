/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

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
    // idlist holds the ownership
}

TEST(SubmissionsAggregator, givenTwoCommandBuffersWhenMergeResourcesIsCalledThenDuplicatesAreEliminated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 2);
    MockGraphicsAllocation alloc3(nullptr, 3);
    MockGraphicsAllocation alloc4(nullptr, 4);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc6(nullptr, 6);

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

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    EXPECT_EQ(0u, totalUsedSize);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    EXPECT_EQ(15u, totalUsedSize);
    totalUsedSize = 0;
    resourcePackage.clear();

    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    EXPECT_EQ(cmdBuffer, submissionsAggregator.peekCommandBuffersList().peekHead());
    EXPECT_EQ(cmdBuffer2, submissionsAggregator.peekCommandBuffersList().peekTail());
    EXPECT_NE(submissionsAggregator.peekCommandBuffersList().peekHead(), submissionsAggregator.peekCommandBuffersList().peekTail());

    EXPECT_EQ(5u, cmdBuffer->surfaces.size());
    EXPECT_EQ(4u, cmdBuffer2->surfaces.size());

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // command buffer 2 is aggregated to command buffer 1
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 2);
    MockGraphicsAllocation alloc3(nullptr, 3);
    MockGraphicsAllocation alloc4(nullptr, 4);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc6(nullptr, 6);
    MockGraphicsAllocation alloc7(nullptr, 7);

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

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // command buffer 3 and 2 is aggregated to command buffer 1
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 2);
    MockGraphicsAllocation alloc3(nullptr, 3);
    MockGraphicsAllocation alloc4(nullptr, 4);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc6(nullptr, 6);
    MockGraphicsAllocation alloc7(nullptr, 7);

    // 14 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    // 12 bytes total , only 7 new
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    // 12 bytes total, only 7 new
    cmdBuffer3->surfaces.push_back(&alloc7);
    cmdBuffer3->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 22;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    submissionsAggregator.recordCommandBuffer(cmdBuffer3);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // command buffer 2 is aggregated to command buffer 1, comand buffer 3 becomes command buffer 2
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 2);
    MockGraphicsAllocation alloc3(nullptr, 3);
    MockGraphicsAllocation alloc4(nullptr, 4);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc6(nullptr, 6);
    MockGraphicsAllocation alloc7(nullptr, 7);

    // 14 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc6);
    cmdBuffer->surfaces.push_back(&alloc5);
    cmdBuffer->surfaces.push_back(&alloc3);
    cmdBuffer->surfaces.push_back(&alloc6);

    // 12 bytes total , only 7 new
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->surfaces.push_back(&alloc5);
    cmdBuffer2->surfaces.push_back(&alloc4);

    // 12 bytes total, only 7 new
    cmdBuffer3->surfaces.push_back(&alloc7);
    cmdBuffer3->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 14;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);
    submissionsAggregator.recordCommandBuffer(cmdBuffer3);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // command buffers not aggregated due to too low limit
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekHead(), cmdBuffer);
    EXPECT_EQ(cmdBuffer->next, cmdBuffer2);
    EXPECT_EQ(submissionsAggregator.peekCommandBuffersList().peekTail(), cmdBuffer3);

    // budget is now larger we can fit everything
    totalMemoryBudget = 28;
    resourcePackage.clear();
    totalUsedSize = 0;

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);
    // all cmd buffers are merged to 1
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 2);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc7(nullptr, 7);

    // 5 bytes consumed
    cmdBuffer->surfaces.push_back(&alloc5);

    // 10 bytes total
    cmdBuffer2->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc2);
    cmdBuffer2->batchBuffer.commandBufferAllocation = &alloc7;

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    EXPECT_EQ(4u, resourcePackage.size());
    EXPECT_EQ(15u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenTwoCommandBufferWhereSecondContainsFirstOnResourceListWhenItIsAggregatedThenResourcePackDoesntContainPrimaryBatch) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    MockGraphicsAllocation cmdBufferAllocation1(nullptr, 1);
    MockGraphicsAllocation cmdBufferAllocation2(nullptr, 2);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;
    cmdBuffer2->batchBuffer.commandBufferAllocation = &cmdBufferAllocation2;

    // cmdBuffer2 has commandBufferAllocation on the surface list
    cmdBuffer2->surfaces.push_back(&cmdBufferAllocation1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    cmdBuffer->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // resource pack shuold have 3 surfaces
    EXPECT_EQ(3u, resourcePackage.size());
    EXPECT_EQ(14u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenTwoCommandBufferWhereSecondContainsTheFirstCommandBufferGraphicsAllocaitonWhenItIsAggregatedThenResourcePackDoesntContainPrimaryBatch) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    MockGraphicsAllocation cmdBufferAllocation1(nullptr, 1);
    MockGraphicsAllocation alloc5(nullptr, 5);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;
    cmdBuffer2->batchBuffer.commandBufferAllocation = &cmdBufferAllocation1;

    // cmdBuffer2 has commandBufferAllocation on the surface list
    cmdBuffer2->surfaces.push_back(&alloc7);
    cmdBuffer->surfaces.push_back(&alloc5);

    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    ResourcePackage resourcePackage;

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);

    // resource pack shuold have 3 surfaces
    EXPECT_EQ(2u, resourcePackage.size());
    EXPECT_EQ(12u, totalUsedSize);
}

TEST(SubmissionsAggregator, givenCommandBuffersRequiringDifferentCoherencySettingWhenAggregateIsCalledThenTheyAreNotAgggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.requiresCoherency = true;
    cmdBuffer2->batchBuffer.requiresCoherency = false;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.throttle = QueueThrottle::LOW;
    cmdBuffer2->batchBuffer.throttle = QueueThrottle::MEDIUM;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);
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

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.low_priority = true;
    cmdBuffer2->batchBuffer.low_priority = false;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);
    EXPECT_EQ(1u, totalUsedSize);
    EXPECT_EQ(1u, resourcePackage.size());
    EXPECT_NE(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(1u, cmdBuffer->inspectionId);
}

TEST(SubmissionsAggregator, WhenAggregatorIsCreatedThenFlushStampIsNotAllocated) {
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer cmdBuffer(*device);
    EXPECT_EQ(nullptr, cmdBuffer.flushStamp->getStampReference());
}

TEST(SubmissionsAggregator, givenMultipleOsContextsWhenAggregatingGraphicsAllocationsThenUseInspectionIdCorrespondingWithOsContextId) {
    SubmissionAggregator submissionsAggregator;
    ResourcePackage resourcePackage;
    const auto totalMemoryBudget = 3u;
    size_t totalUsedSize = 0;
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer0 = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer1 = new CommandBuffer(*device);

    MockGraphicsAllocation alloc0(nullptr, 1);
    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc2(nullptr, 1);
    MockGraphicsAllocation alloc3(nullptr, 1);

    cmdBuffer0->surfaces.push_back(&alloc0);
    cmdBuffer0->surfaces.push_back(&alloc1);
    cmdBuffer1->surfaces.push_back(&alloc2);
    cmdBuffer1->surfaces.push_back(&alloc3);

    submissionsAggregator.recordCommandBuffer(cmdBuffer0);
    submissionsAggregator.recordCommandBuffer(cmdBuffer1);

    EXPECT_EQ(0u, alloc0.getInspectionId(1u));
    EXPECT_EQ(0u, alloc1.getInspectionId(1u));
    EXPECT_EQ(0u, alloc2.getInspectionId(1u));
    EXPECT_EQ(0u, alloc3.getInspectionId(1u));
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 1u);
    EXPECT_EQ(1u, alloc0.getInspectionId(1u));
    EXPECT_EQ(1u, alloc1.getInspectionId(1u));
    EXPECT_EQ(1u, alloc2.getInspectionId(1u));
    EXPECT_EQ(1u, alloc3.getInspectionId(1u));
}

TEST(SubmissionsAggregator, givenMultipleOsContextsWhenAggregatingGraphicsAllocationsThenDoNotUpdateInspectionIdsOfOtherContexts) {
    SubmissionAggregator submissionsAggregator;
    ResourcePackage resourcePackage;
    const auto totalMemoryBudget = 2u;
    size_t totalUsedSize = 0;
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer0 = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer1 = new CommandBuffer(*device);

    MockGraphicsAllocation alloc0(nullptr, 1);
    MockGraphicsAllocation alloc1(nullptr, 1);

    cmdBuffer0->surfaces.push_back(&alloc0);
    cmdBuffer0->surfaces.push_back(&alloc1);

    submissionsAggregator.recordCommandBuffer(cmdBuffer0);
    submissionsAggregator.recordCommandBuffer(cmdBuffer1);
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 1u);

    for (auto osContextId = 0u; osContextId < alloc1.usageInfos.size(); osContextId++) {
        if (osContextId != 1u) {
            EXPECT_EQ(0u, alloc0.getInspectionId(osContextId));
        }
    }
    for (auto osContextId = 0u; osContextId < alloc0.usageInfos.size(); osContextId++) {
        if (osContextId != 1u) {
            EXPECT_EQ(0u, alloc0.getInspectionId(osContextId));
        }
    }
}

TEST(SubmissionsAggregator, givenCommandBuffersRequiringDifferentSliceCountSettingWhenAggregateIsCalledThenTheyAreNotAgggregated) {
    MockSubmissionAggregator submissionsAggregator;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    CommandBuffer *cmdBuffer = new CommandBuffer(*device);
    CommandBuffer *cmdBuffer2 = new CommandBuffer(*device);

    MockGraphicsAllocation alloc1(nullptr, 1);
    MockGraphicsAllocation alloc7(nullptr, 7);

    cmdBuffer->batchBuffer.sliceCount = 1;
    cmdBuffer2->batchBuffer.sliceCount = 2;

    cmdBuffer->surfaces.push_back(&alloc1);
    cmdBuffer2->surfaces.push_back(&alloc7);

    submissionsAggregator.recordCommandBuffer(cmdBuffer);
    submissionsAggregator.recordCommandBuffer(cmdBuffer2);

    ResourcePackage resourcePackage;
    size_t totalUsedSize = 0;
    size_t totalMemoryBudget = 200;
    submissionsAggregator.aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, 0u);
    EXPECT_EQ(1u, totalUsedSize);
    EXPECT_EQ(1u, resourcePackage.size());
    EXPECT_NE(cmdBuffer->inspectionId, cmdBuffer2->inspectionId);
    EXPECT_EQ(1u, cmdBuffer->inspectionId);
}

struct SubmissionsAggregatorTests : public ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        context.reset(new MockContext(device.get()));
    }

    void overrideCsr(CommandStreamReceiver *newCsr) {
        device->resetCommandStreamReceiver(newCsr);
        newCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
};

HWTEST_F(SubmissionsAggregatorTests, givenMultipleQueuesWhenCmdBuffersAreRecordedThenAssignFlushStampObjFromCmdQueue) {
    MockKernelWithInternals kernel(*device.get());
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0, false);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0, false);
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    size_t gws = 1;

    overrideCsr(mockCsr);

    auto expectRefCounts = [&](int32_t cmdQRef1, int32_t cmdQRef2) {
        EXPECT_EQ(cmdQRef1, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());
        EXPECT_EQ(cmdQRef2, cmdQ2.flushStamp->getStampReference()->getRefInternalCount());
    };

    expectRefCounts(1, 1);
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
    expectRefCounts(2, 1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
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
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0, false);
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    size_t gws = 1;

    overrideCsr(mockCsr);

    cl_event event1;
    EXPECT_EQ(1, cmdQ1.flushStamp->getStampReference()->getRefInternalCount());
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event1);
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
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0, false);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0, false);
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    size_t gws = 1;

    overrideCsr(mockCsr);
    mockCsr->taskCount = 5;
    mockCsr->flushStamp->setStamp(5);

    cl_event event1, event2;
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event2);

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
    CommandQueueHw<FamilyType> cmdQ1(context.get(), device.get(), 0, false);
    CommandQueueHw<FamilyType> cmdQ2(context.get(), device.get(), 0, false);
    auto mockCsr = new MockCsrHw2<FamilyType>(*device->executionEnvironment, device->getRootDeviceIndex(), device->getDeviceBitfield());
    size_t gws = 1;

    overrideCsr(mockCsr);
    mockCsr->taskCount = 5;
    mockCsr->flushStamp->setStamp(5);

    cl_event event1, event2;
    cmdQ1.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event1);
    cmdQ2.enqueueKernel(kernel, 1, nullptr, &gws, nullptr, 0, nullptr, &event2);

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

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "submissions_aggregator.h"

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/helpers/flush_stamp.h"

void NEO::SubmissionAggregator::recordCommandBuffer(CommandBuffer *commandBuffer) {
    this->cmdBuffers.pushTailOne(*commandBuffer);
}

void NEO::SubmissionAggregator::aggregateCommandBuffers(ResourcePackage &resourcePackage, size_t &totalUsedSize, size_t totalMemoryBudget, uint32_t osContextId) {
    auto primaryCommandBuffer = this->cmdBuffers.peekHead();
    auto currentInspection = this->inspectionId;

    if (!primaryCommandBuffer) {
        return;
    }
    auto primaryBatchGraphicsAllocation = primaryCommandBuffer->batchBuffer.commandBufferAllocation;

    this->inspectionId++;
    primaryCommandBuffer->inspectionId = currentInspection;

    //primary command buffers must fix to budget
    for (auto &graphicsAllocation : primaryCommandBuffer->surfaces) {
        if (graphicsAllocation->getInspectionId(osContextId) < currentInspection) {
            graphicsAllocation->setInspectionId(currentInspection, osContextId);
            resourcePackage.push_back(graphicsAllocation);
            totalUsedSize += graphicsAllocation->getUnderlyingBufferSize();
        }
    }

    //check if we have anything for merge
    if (!primaryCommandBuffer->next) {
        return;
    }

    //check if next cmd buffer is compatible
    if (primaryCommandBuffer->next->batchBuffer.requiresCoherency != primaryCommandBuffer->batchBuffer.requiresCoherency) {
        return;
    }

    if (primaryCommandBuffer->next->batchBuffer.low_priority != primaryCommandBuffer->batchBuffer.low_priority) {
        return;
    }

    if (primaryCommandBuffer->next->batchBuffer.throttle != primaryCommandBuffer->batchBuffer.throttle) {
        return;
    }

    auto nextCommandBuffer = primaryCommandBuffer->next;
    ResourcePackage newResources;

    while (nextCommandBuffer) {
        size_t nextCommandBufferNewResourcesSize = 0;
        //evaluate if buffer fits
        for (auto &graphicsAllocation : nextCommandBuffer->surfaces) {
            if (graphicsAllocation == primaryBatchGraphicsAllocation) {
                continue;
            }
            if (graphicsAllocation->getInspectionId(osContextId) < currentInspection) {
                graphicsAllocation->setInspectionId(currentInspection, osContextId);
                newResources.push_back(graphicsAllocation);
                nextCommandBufferNewResourcesSize += graphicsAllocation->getUnderlyingBufferSize();
            }
        }

        if (nextCommandBuffer->batchBuffer.commandBufferAllocation && (nextCommandBuffer->batchBuffer.commandBufferAllocation != primaryBatchGraphicsAllocation)) {
            if (nextCommandBuffer->batchBuffer.commandBufferAllocation->getInspectionId(osContextId) < currentInspection) {
                nextCommandBuffer->batchBuffer.commandBufferAllocation->setInspectionId(currentInspection, osContextId);
                newResources.push_back(nextCommandBuffer->batchBuffer.commandBufferAllocation);
                nextCommandBufferNewResourcesSize += nextCommandBuffer->batchBuffer.commandBufferAllocation->getUnderlyingBufferSize();
            }
        }

        if (nextCommandBufferNewResourcesSize + totalUsedSize <= totalMemoryBudget) {
            auto currentNode = nextCommandBuffer;
            nextCommandBuffer = nextCommandBuffer->next;
            totalUsedSize += nextCommandBufferNewResourcesSize;
            currentNode->inspectionId = currentInspection;

            for (auto &newResource : newResources) {
                resourcePackage.push_back(newResource);
            }
            newResources.clear();
        } else {
            break;
        }
    }
}

NEO::BatchBuffer::BatchBuffer(GraphicsAllocation *commandBufferAllocation, size_t startOffset, size_t chainedBatchBufferStartOffset, GraphicsAllocation *chainedBatchBuffer, bool requiresCoherency, bool lowPriority, QueueThrottle throttle, size_t usedSize, LinearStream *stream) : commandBufferAllocation(commandBufferAllocation), startOffset(startOffset), chainedBatchBufferStartOffset(chainedBatchBufferStartOffset), chainedBatchBuffer(chainedBatchBuffer), requiresCoherency(requiresCoherency), low_priority(lowPriority), throttle(throttle), usedSize(usedSize), stream(stream) {
}

NEO::CommandBuffer::CommandBuffer(Device &device) : device(device) {
    flushStamp.reset(new FlushStampTracker(false));
}

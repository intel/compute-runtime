/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "submissions_aggregator.h"

#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/graphics_allocation.h"

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

    ResourcePackage newResources;

    auto isTrackedByPointer = [&](GraphicsAllocation *graphicsAllocation) {
        if (graphicsAllocation == nullptr) {
            return false;
        }

        if (std::find(resourcePackage.begin(), resourcePackage.end(), graphicsAllocation) != resourcePackage.end()) {
            return true;
        }

        return std::find(newResources.begin(), newResources.end(), graphicsAllocation) != newResources.end();
    };

    // primary command buffers must fit to budget
    for (auto &graphicsAllocation : primaryCommandBuffer->surfaces) {
        if (graphicsAllocation == nullptr) {
            continue;
        }

        if (graphicsAllocation->isView()) {
            // Different views of one parent share inspectionId.
            // Track views by pointer so we don't drop distinct subranges.
            if (isTrackedByPointer(graphicsAllocation)) {
                continue;
            }

            resourcePackage.push_back(graphicsAllocation);
            totalUsedSize += graphicsAllocation->getUnderlyingBufferSize();
            continue;
        }

        if (graphicsAllocation->getInspectionId(osContextId) < currentInspection) {
            graphicsAllocation->setInspectionId(currentInspection, osContextId);
            resourcePackage.push_back(graphicsAllocation);
            totalUsedSize += graphicsAllocation->getUnderlyingBufferSize();
        }
    }

    // check if we have anything for merge
    if (!primaryCommandBuffer->next) {
        return;
    }

    if (primaryCommandBuffer->next->batchBuffer.lowPriority != primaryCommandBuffer->batchBuffer.lowPriority) {
        return;
    }

    if (primaryCommandBuffer->next->batchBuffer.throttle != primaryCommandBuffer->batchBuffer.throttle) {
        return;
    }

    if (primaryCommandBuffer->next->batchBuffer.sliceCount != primaryCommandBuffer->batchBuffer.sliceCount) {
        return;
    }

    auto nextCommandBuffer = primaryCommandBuffer->next;
    auto addMergedResource = [&](GraphicsAllocation *graphicsAllocation, size_t &newResourcesSize) {
        if (graphicsAllocation == nullptr) {
            return;
        }

        if (graphicsAllocation->isView()) {
            // Different views of one parent share inspectionId.
            // Track views by pointer so we don't drop distinct subranges.
            if (isTrackedByPointer(graphicsAllocation)) {
                return;
            }

            newResources.push_back(graphicsAllocation);
            newResourcesSize += graphicsAllocation->getUnderlyingBufferSize();
            return;
        }

        if (graphicsAllocation->getInspectionId(osContextId) < currentInspection) {
            graphicsAllocation->setInspectionId(currentInspection, osContextId);
            newResources.push_back(graphicsAllocation);
            newResourcesSize += graphicsAllocation->getUnderlyingBufferSize();
        }
    };

    while (nextCommandBuffer) {
        size_t nextCommandBufferNewResourcesSize = 0;
        // evaluate if buffer fits
        for (auto &graphicsAllocation : nextCommandBuffer->surfaces) {
            if (graphicsAllocation == primaryBatchGraphicsAllocation) {
                continue;
            }
            addMergedResource(graphicsAllocation, nextCommandBufferNewResourcesSize);
        }

        if (nextCommandBuffer->batchBuffer.commandBufferAllocation && (nextCommandBuffer->batchBuffer.commandBufferAllocation != primaryBatchGraphicsAllocation)) {
            addMergedResource(nextCommandBuffer->batchBuffer.commandBufferAllocation, nextCommandBufferNewResourcesSize);
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

NEO::BatchBuffer::BatchBuffer(GraphicsAllocation *commandBufferAllocation, size_t startOffset,
                              size_t chainedBatchBufferStartOffset, uint64_t taskStartAddress, GraphicsAllocation *chainedBatchBuffer,
                              bool lowPriority, QueueThrottle throttle, uint64_t sliceCount,
                              size_t usedSize, LinearStream *stream, void *endCmdPtr, uint32_t numCsrClients, bool hasStallingCmds,
                              bool hasRelaxedOrderingDependencies, bool dispatchMonitorFence, bool taskCountUpdateOnly)
    : commandBufferAllocation(commandBufferAllocation), startOffset(startOffset),
      chainedBatchBufferStartOffset(chainedBatchBufferStartOffset), taskStartAddress(taskStartAddress), chainedBatchBuffer(chainedBatchBuffer),
      lowPriority(lowPriority),
      throttle(throttle), sliceCount(sliceCount),
      usedSize(usedSize), stream(stream), endCmdPtr(endCmdPtr), numCsrClients(numCsrClients), hasStallingCmds(hasStallingCmds),
      hasRelaxedOrderingDependencies(hasRelaxedOrderingDependencies), dispatchMonitorFence(dispatchMonitorFence), taskCountUpdateOnly(taskCountUpdateOnly) {}

NEO::CommandBuffer::CommandBuffer(Device &device) : device(device) {
    flushStamp.reset(new FlushStampTracker(false));
}

NEO::CommandBuffer::~CommandBuffer() = default;

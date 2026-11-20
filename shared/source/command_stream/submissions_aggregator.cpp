/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "submissions_aggregator.h"

#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include <algorithm>

void NEO::SubmissionAggregator::recordCommandBuffer(std::unique_ptr<CommandBuffer> commandBuffer) {
    this->cmdBuffers.push_back(std::move(commandBuffer));
}

void NEO::SubmissionAggregator::aggregateCommandBuffers(size_t primaryCommandBufferIndex, ResourcePackage &resourcePackage, size_t &totalUsedSize, size_t totalMemoryBudget, uint32_t osContextId) {
    auto currentInspection = this->inspectionId;

    if (primaryCommandBufferIndex >= this->cmdBuffers.size()) {
        return;
    }
    auto primaryCommandBuffer = this->cmdBuffers[primaryCommandBufferIndex].get();
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
    auto nextCommandBufferIndex = primaryCommandBufferIndex + 1;
    if (nextCommandBufferIndex == this->cmdBuffers.size()) {
        return;
    }

    const auto &firstMergeCandidate = *this->cmdBuffers[nextCommandBufferIndex];
    if (firstMergeCandidate.batchBuffer.lowPriority != primaryCommandBuffer->batchBuffer.lowPriority) {
        return;
    }

    if (firstMergeCandidate.batchBuffer.throttle != primaryCommandBuffer->batchBuffer.throttle) {
        return;
    }

    if (firstMergeCandidate.batchBuffer.sliceCount != primaryCommandBuffer->batchBuffer.sliceCount) {
        return;
    }

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

    for (; nextCommandBufferIndex < this->cmdBuffers.size(); nextCommandBufferIndex++) {
        auto nextCommandBuffer = this->cmdBuffers[nextCommandBufferIndex].get();
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
            totalUsedSize += nextCommandBufferNewResourcesSize;
            nextCommandBuffer->inspectionId = currentInspection;

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
    : commandBufferAllocation(commandBufferAllocation), chainedBatchBuffer(chainedBatchBuffer),
      stream(stream), endCmdPtr(endCmdPtr),
      startOffset(startOffset), chainedBatchBufferStartOffset(chainedBatchBufferStartOffset), usedSize(usedSize),
      taskStartAddress(taskStartAddress), sliceCount(sliceCount),
      throttle(throttle), numCsrClients(numCsrClients),
      lowPriority(lowPriority), hasStallingCmds(hasStallingCmds), hasRelaxedOrderingDependencies(hasRelaxedOrderingDependencies),
      dispatchMonitorFence(dispatchMonitorFence), taskCountUpdateOnly(taskCountUpdateOnly) {}

NEO::CommandBuffer::CommandBuffer(Device &device) : device(device) {
    flushStamp.reset(new FlushStampTracker(false));
}

NEO::CommandBuffer::~CommandBuffer() = default;

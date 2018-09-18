/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/idlist.h"
#include "runtime/utilities/stackvec.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/memory_manager/residency_container.h"
#include <vector>
namespace OCLRT {
class Device;
class Event;
class FlushStampTracker;
class GraphicsAllocation;

struct BatchBuffer {
    BatchBuffer(GraphicsAllocation *commandBufferAllocation,
                size_t startOffset,
                size_t chainedBatchBufferStartOffset,
                GraphicsAllocation *chainedBatchBuffer,
                bool requiresCoherency,
                bool lowPriority,
                QueueThrottle throttle,
                size_t usedSize,
                LinearStream *stream);
    BatchBuffer() {}
    GraphicsAllocation *commandBufferAllocation = nullptr;
    size_t startOffset = 0u;
    size_t chainedBatchBufferStartOffset = 0u;
    GraphicsAllocation *chainedBatchBuffer = nullptr;
    bool requiresCoherency = false;
    bool low_priority = false;
    QueueThrottle throttle = QueueThrottle::MEDIUM;
    size_t usedSize = 0u;

    //only used in drm csr in gem close worker active mode
    LinearStream *stream = nullptr;
};

struct CommandBuffer : public IDNode<CommandBuffer> {
    CommandBuffer(Device &);
    ResidencyContainer surfaces;
    BatchBuffer batchBuffer;
    void *batchBufferEndLocation = nullptr;
    uint32_t inspectionId = 0;
    uint32_t taskCount = 0u;
    void *pipeControlThatMayBeErasedLocation = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    std::unique_ptr<FlushStampTracker> flushStamp;
    Device &device;
};

struct CommandBufferList : public IDList<CommandBuffer, false, true, false> {};

using ResourcePackage = StackVec<GraphicsAllocation *, 128>;

class SubmissionAggregator {
  public:
    void recordCommandBuffer(CommandBuffer *commandBuffer);
    void aggregateCommandBuffers(ResourcePackage &resourcePackage, size_t &totalUsedSize, size_t totalMemoryBudget);
    CommandBufferList &peekCmdBufferList() { return cmdBuffers; }

  protected:
    CommandBufferList cmdBuffers;
    uint32_t inspectionId = 1;
};
} // namespace OCLRT

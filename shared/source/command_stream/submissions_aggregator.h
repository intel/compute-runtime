/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/utilities/idlist.h"

#include <vector>
namespace NEO {
class Device;
class Event;
class FlushStampTracker;
class GraphicsAllocation;

struct PagingFenceSemaphoreInfo {
    bool requiresBlockingResidencyHandling = true;
    uint64_t pagingFenceValue = 0u;

    bool requiresProgrammingSemaphore() {
        return !requiresBlockingResidencyHandling && pagingFenceValue > 0u;
    }
};

struct BatchBuffer {
    BatchBuffer(GraphicsAllocation *commandBufferAllocation,
                size_t startOffset,
                size_t chainedBatchBufferStartOffset,
                uint64_t taskStartAddress,
                GraphicsAllocation *chainedBatchBuffer,
                bool lowPriority,
                QueueThrottle throttle,
                uint64_t sliceCount,
                size_t usedSize,
                LinearStream *stream,
                void *endCmdPtr,
                uint32_t numCsrClients,
                bool hasStallingCmds,
                bool hasRelaxedOrderingDependencies,
                bool dispatchMonitorFence,
                bool taskCountUpdateOnly);
    BatchBuffer() {}
    GraphicsAllocation *commandBufferAllocation = nullptr;
    ResidencyContainer *allocationsForResidency = nullptr;
    size_t startOffset = 0u;
    size_t chainedBatchBufferStartOffset = 0u;
    uint64_t taskStartAddress = 0; // if task not available, use CSR stream

    GraphicsAllocation *chainedBatchBuffer = nullptr;
    bool lowPriority = false;
    QueueThrottle throttle = QueueThrottle::MEDIUM;
    uint64_t sliceCount = QueueSliceCount::defaultSliceCount;
    size_t usedSize = 0u;

    // only used in drm csr in gem close worker active mode
    LinearStream *stream = nullptr;
    void *endCmdPtr = nullptr;
    uint32_t numCsrClients = 0;

    PagingFenceSemaphoreInfo pagingFenceSemInfo{};

    bool hasStallingCmds = false;
    bool hasRelaxedOrderingDependencies = false;
    bool disableFlatRingBuffer = false;
    bool dispatchMonitorFence = false;
    bool taskCountUpdateOnly = false;
};

struct CommandBuffer : public IDNode<CommandBuffer>, NEO::NonCopyableAndNonMovableClass {
    CommandBuffer(Device &device);
    ~CommandBuffer() override;
    ResidencyContainer surfaces;
    BatchBuffer batchBuffer;
    void *batchBufferEndLocation = nullptr;
    uint32_t inspectionId = 0;
    TaskCountType taskCount = 0u;
    void *pipeControlThatMayBeErasedLocation = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    PipeControlArgs epiloguePipeControlArgs;
    std::unique_ptr<FlushStampTracker> flushStamp;
    Device &device;
};

static_assert(NEO::NonCopyableAndNonMovable<CommandBuffer>);

struct CommandBufferList : public IDList<CommandBuffer, false, true, false> {};

using ResourcePackage = StackVec<GraphicsAllocation *, 128>;

class SubmissionAggregator {
  public:
    void recordCommandBuffer(CommandBuffer *commandBuffer);
    void aggregateCommandBuffers(ResourcePackage &resourcePackage, size_t &totalUsedSize, size_t totalMemoryBudget, uint32_t osContextId);
    CommandBufferList &peekCmdBufferList() { return cmdBuffers; }

  protected:
    CommandBufferList cmdBuffers;
    uint32_t inspectionId = 1;
};
} // namespace NEO

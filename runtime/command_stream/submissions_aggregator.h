/*
* Copyright (c) 2017 - 2018, Intel Corporation
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
#pragma once
#include "runtime/utilities/idlist.h"
#include "runtime/utilities/stackvec.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/memory_manager/residency_container.h"
#include <vector>
namespace OCLRT {
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
    CommandBuffer();
    ResidencyContainer surfaces;
    BatchBuffer batchBuffer;
    void *batchBufferEndLocation = nullptr;
    uint32_t inspectionId = 0;
    uint32_t taskCount = 0u;
    void *pipeControlThatMayBeErasedLocation = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    std::unique_ptr<FlushStampTracker> flushStamp;
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
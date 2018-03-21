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

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/event/event.h"
#include "runtime/event/event_builder.h"

namespace OCLRT {
// Global table of CommandStreamReceiver factories for HW and tests
CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE] = {};

CommandStreamReceiver::CommandStreamReceiver() {
    latestSentStatelessMocsConfig = CacheSettings::unknownMocs;
    submissionAggregator.reset(new SubmissionAggregator());
    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }
    flushStamp.reset(new FlushStampTracker(true));
}

CommandStreamReceiver::~CommandStreamReceiver() {
    cleanupResources();
}

void CommandStreamReceiver::makeResident(GraphicsAllocation &gfxAllocation) {
    auto submissionTaskCount = this->taskCount + 1;
    if (gfxAllocation.residencyTaskCount < (int)submissionTaskCount) {
        getMemoryManager()->pushAllocationForResidency(&gfxAllocation);
        gfxAllocation.taskCount = submissionTaskCount;
        if (gfxAllocation.residencyTaskCount == ObjectNotResident) {
            this->totalMemoryUsed += gfxAllocation.getUnderlyingBufferSize();
        }
    }
    gfxAllocation.residencyTaskCount = submissionTaskCount;
}

void CommandStreamReceiver::processEviction() {
    getMemoryManager()->clearEvictionAllocations();
}

void CommandStreamReceiver::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.residencyTaskCount != ObjectNotResident) {
        makeCoherent(gfxAllocation.getUnderlyingBuffer(), gfxAllocation.getUnderlyingBufferSize());
        getMemoryManager()->pushAllocationForEviction(&gfxAllocation);
    }

    gfxAllocation.residencyTaskCount = ObjectNotResident;
}

void CommandStreamReceiver::makeSurfacePackNonResident(ResidencyContainer *allocationsForResidency) {
    auto &residencyAllocations = allocationsForResidency ? *allocationsForResidency : this->getMemoryManager()->getResidencyAllocations();
    for (auto &surface : residencyAllocations) {
        this->makeNonResident(*surface);
    }
    if (allocationsForResidency) {
        residencyAllocations.clear();
    } else {
        this->getMemoryManager()->clearResidencyAllocations();
    }
    this->processEviction();
}

GraphicsAllocation *CommandStreamReceiver::createAllocationAndHandleResidency(const void *address, size_t size, bool addToDefferedDeleteList) {
    GraphicsAllocation *graphicsAllocation = getMemoryManager()->allocateGraphicsMemory(size, address);
    makeResident(*graphicsAllocation);
    if (addToDefferedDeleteList) {
        getMemoryManager()->storeAllocation(std::unique_ptr<GraphicsAllocation>(graphicsAllocation), TEMPORARY_ALLOCATION);
    }
    if (!graphicsAllocation->isL3Capable()) {
        disableL3Cache = true;
    }

    return graphicsAllocation;
}

void CommandStreamReceiver::makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation) {
    makeResident(*gfxAllocation);
    if (!gfxAllocation->isL3Capable()) {
        setDisableL3Cache(true);
    }
}

void CommandStreamReceiver::waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationType) {

    auto address = getTagAddress();
    if (address && requiredTaskCount != (unsigned int)-1) {
        while (*address < requiredTaskCount)
            ;
    }

    getMemoryManager()->cleanAllocationList(requiredTaskCount, allocationType);
}

MemoryManager *CommandStreamReceiver::getMemoryManager() {
    return memoryManager;
}

void CommandStreamReceiver::setMemoryManager(MemoryManager *mm) {
    memoryManager = mm;
}

LinearStream &CommandStreamReceiver::getCS(size_t minRequiredSize) {
    auto memoryManager = this->getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    if (commandStream.getAvailableSpace() < minRequiredSize) {
        // Make sure we have enough room for a MI_BATCH_BUFFER_END and any padding.
        // Currently reserving 64bytes (cacheline) which should be more than enough.
        static const size_t sizeForSubmission = MemoryConstants::cacheLineSize;
        minRequiredSize += sizeForSubmission;
        // If not, allocate a new block. allocate full pages
        minRequiredSize = alignUp(minRequiredSize, MemoryConstants::pageSize);

        auto requiredSize = minRequiredSize + CSRequirements::csOverfetchSize;

        auto allocation = memoryManager->obtainReusableAllocation(requiredSize).release();
        if (!allocation) {
            allocation = memoryManager->allocateGraphicsMemory(requiredSize, MemoryConstants::pageSize);
        }

        //pass current allocation to reusable list
        if (commandStream.getCpuBase()) {
            memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream.getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }

        commandStream.replaceBuffer(allocation->getUnderlyingBuffer(), minRequiredSize - sizeForSubmission);
        commandStream.replaceGraphicsAllocation(allocation);
    }

    return commandStream;
}

void CommandStreamReceiver::cleanupResources() {
    auto memoryManager = this->getMemoryManager();
    if (!memoryManager)
        return;

    waitForTaskCountAndCleanAllocationList(this->latestFlushedTaskCount, TEMPORARY_ALLOCATION);
    waitForTaskCountAndCleanAllocationList(this->latestFlushedTaskCount, REUSABLE_ALLOCATION);

    if (scratchAllocation) {
        memoryManager->freeGraphicsMemory(scratchAllocation);
        scratchAllocation = nullptr;
    }

    if (debugSurface) {
        memoryManager->freeGraphicsMemory(debugSurface);
        debugSurface = nullptr;
    }

    if (commandStream.getCpuBase()) {
        memoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());
        commandStream.replaceGraphicsAllocation(nullptr);
        commandStream.replaceBuffer(nullptr, 0);
    }
}

bool CommandStreamReceiver::waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    int64_t timeDiff = 0;

    uint32_t latestSentTaskCount = this->latestFlushedTaskCount;
    if (latestSentTaskCount < taskCountToWait) {
        this->flushBatchedSubmissions();
    }

    time1 = std::chrono::high_resolution_clock::now();
    while (*getTagAddress() < taskCountToWait && timeDiff <= timeoutMicroseconds) {
        if (enableTimeout) {
            time2 = std::chrono::high_resolution_clock::now();
            timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(time2 - time1).count();
        }
    }
    if (*getTagAddress() >= taskCountToWait) {
        if (gtpinIsGTPinInitialized()) {
            gtpinNotifyTaskCompletion(taskCountToWait);
        }
        return true;
    }
    return false;
}

void CommandStreamReceiver::setTagAllocation(GraphicsAllocation *allocation) {
    this->tagAllocation = allocation;
    this->tagAddress = allocation ? reinterpret_cast<uint32_t *>(allocation->getUnderlyingBuffer()) : nullptr;
}

void CommandStreamReceiver::setRequiredScratchSize(uint32_t newRequiredScratchSize) {
    if (newRequiredScratchSize > requiredScratchSize) {
        requiredScratchSize = newRequiredScratchSize;
    }
}

size_t CommandStreamReceiver::getInstructionHeapCmdStreamReceiverReservedSize() const {
    return PreemptionHelper::getInstructionHeapSipKernelReservedSize(*memoryManager->device);
}

void CommandStreamReceiver::initializeInstructionHeapCmdStreamReceiverReservedBlock(LinearStream &ih) const {
    return PreemptionHelper::initializeInstructionHeapSipKernelReservedBlock(ih, *memoryManager->device);
}

GraphicsAllocation *CommandStreamReceiver::allocateDebugSurface(size_t size) {
    UNRECOVERABLE_IF(debugSurface != nullptr);
    debugSurface = memoryManager->allocateGraphicsMemory(size);
    return debugSurface;
}

} // namespace OCLRT

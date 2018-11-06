/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/flush_stamp.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_interface.h"

namespace OCLRT {
// Global table of CommandStreamReceiver factories for HW and tests
CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE] = {};

CommandStreamReceiver::CommandStreamReceiver(ExecutionEnvironment &executionEnvironment)
    : executionEnvironment(executionEnvironment) {
    residencyAllocations.reserve(20);

    latestSentStatelessMocsConfig = CacheSettings::unknownMocs;
    submissionAggregator.reset(new SubmissionAggregator());
    if (DebugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)DebugManager.flags.CsrDispatchMode.get();
    }
    flushStamp.reset(new FlushStampTracker(true));
    for (int i = 0; i < IndirectHeap::NUM_TYPES; ++i) {
        indirectHeap[i] = nullptr;
    }
    internalAllocationStorage = std::make_unique<InternalAllocationStorage>(*this);
}

CommandStreamReceiver::~CommandStreamReceiver() {
    for (int i = 0; i < IndirectHeap::NUM_TYPES; ++i) {
        if (indirectHeap[i] != nullptr) {
            auto allocation = indirectHeap[i]->getGraphicsAllocation();
            if (allocation != nullptr) {
                internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
            }
            delete indirectHeap[i];
        }
    }
    cleanupResources();

    internalAllocationStorage->cleanAllocationList(-1, REUSABLE_ALLOCATION);
    internalAllocationStorage->cleanAllocationList(-1, TEMPORARY_ALLOCATION);
}

void CommandStreamReceiver::makeResident(GraphicsAllocation &gfxAllocation) {
    int submissionTaskCount = this->taskCount + 1;
    if (gfxAllocation.getResidencyTaskCount(deviceIndex) < submissionTaskCount) {
        this->getResidencyAllocations().push_back(&gfxAllocation);
        gfxAllocation.updateTaskCount(submissionTaskCount, deviceIndex);
        if (!gfxAllocation.isResident(deviceIndex)) {
            this->totalMemoryUsed += gfxAllocation.getUnderlyingBufferSize();
        }
    }
    gfxAllocation.updateResidencyTaskCount(submissionTaskCount, deviceIndex);
}

void CommandStreamReceiver::processEviction(OsContext &osContext) {
    this->getEvictionAllocations().clear();
}

void CommandStreamReceiver::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.isResident(deviceIndex)) {
        makeCoherent(gfxAllocation);
        if (gfxAllocation.peekEvictable()) {
            this->getEvictionAllocations().push_back(&gfxAllocation);
        } else {
            gfxAllocation.setEvictable(true);
        }
    }

    gfxAllocation.resetResidencyTaskCount(this->deviceIndex);
}

void CommandStreamReceiver::makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    this->waitBeforeMakingNonResidentWhenRequired();

    for (auto &surface : allocationsForResidency) {
        this->makeNonResident(*surface);
    }
    allocationsForResidency.clear();
    this->processEviction(osContext);
}

void CommandStreamReceiver::makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation) {
    makeResident(*gfxAllocation);
    if (!gfxAllocation->isL3Capable()) {
        setDisableL3Cache(true);
    }
}

void CommandStreamReceiver::waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage) {
    auto address = getTagAddress();
    if (address && requiredTaskCount != ObjectNotUsed) {
        while (*address < requiredTaskCount)
            ;
    }
    internalAllocationStorage->cleanAllocationList(requiredTaskCount, allocationUsage);
}

MemoryManager *CommandStreamReceiver::getMemoryManager() const {
    DEBUG_BREAK_IF(!executionEnvironment.memoryManager);
    return executionEnvironment.memoryManager.get();
}

LinearStream &CommandStreamReceiver::getCS(size_t minRequiredSize) {
    if (commandStream.getAvailableSpace() < minRequiredSize) {
        // Make sure we have enough room for a MI_BATCH_BUFFER_END and any padding.
        // Currently reserving 64bytes (cacheline) which should be more than enough.
        static const size_t sizeForSubmission = MemoryConstants::cacheLineSize;
        minRequiredSize += sizeForSubmission;
        // If not, allocate a new block. allocate full pages
        minRequiredSize = alignUp(minRequiredSize, MemoryConstants::pageSize);

        auto requiredSize = minRequiredSize + CSRequirements::csOverfetchSize;

        auto allocation = internalAllocationStorage->obtainReusableAllocation(requiredSize, false).release();
        if (!allocation) {
            allocation = getMemoryManager()->allocateGraphicsMemory(requiredSize);
        }

        allocation->setAllocationType(GraphicsAllocation::AllocationType::LINEAR_STREAM);

        //pass current allocation to reusable list
        if (commandStream.getCpuBase()) {
            internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream.getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }

        commandStream.replaceBuffer(allocation->getUnderlyingBuffer(), minRequiredSize - sizeForSubmission);
        commandStream.replaceGraphicsAllocation(allocation);
    }

    return commandStream;
}

void CommandStreamReceiver::cleanupResources() {
    if (!getMemoryManager())
        return;

    waitForTaskCountAndCleanAllocationList(this->latestFlushedTaskCount, TEMPORARY_ALLOCATION);
    waitForTaskCountAndCleanAllocationList(this->latestFlushedTaskCount, REUSABLE_ALLOCATION);

    if (scratchAllocation) {
        getMemoryManager()->freeGraphicsMemory(scratchAllocation);
        scratchAllocation = nullptr;
    }

    if (debugSurface) {
        getMemoryManager()->freeGraphicsMemory(debugSurface);
        debugSurface = nullptr;
    }

    if (commandStream.getCpuBase()) {
        getMemoryManager()->freeGraphicsMemory(commandStream.getGraphicsAllocation());
        commandStream.replaceGraphicsAllocation(nullptr);
        commandStream.replaceBuffer(nullptr, 0);
    }

    if (tagAllocation) {
        getMemoryManager()->freeGraphicsMemory(tagAllocation);
        tagAllocation = nullptr;
        tagAddress = nullptr;
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
        std::this_thread::yield();
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

void CommandStreamReceiver::initProgrammingFlags() {
    isPreambleSent = false;
    GSBAFor32BitProgrammed = false;
    mediaVfeStateDirty = true;
    lastVmeSubslicesConfig = false;

    lastSentL3Config = 0;
    lastSentCoherencyRequest = -1;
    lastMediaSamplerConfig = -1;
    lastPreemptionMode = PreemptionMode::Initial;
    latestSentStatelessMocsConfig = 0;
}

ResidencyContainer &CommandStreamReceiver::getResidencyAllocations() {
    return this->residencyAllocations;
}

ResidencyContainer &CommandStreamReceiver::getEvictionAllocations() {
    return this->evictionAllocations;
}

void CommandStreamReceiver::activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) {}

GraphicsAllocation *CommandStreamReceiver::allocateDebugSurface(size_t size) {
    UNRECOVERABLE_IF(debugSurface != nullptr);
    debugSurface = getMemoryManager()->allocateGraphicsMemory(size);
    return debugSurface;
}

IndirectHeap &CommandStreamReceiver::getIndirectHeap(IndirectHeap::Type heapType,
                                                     size_t minRequiredSize) {
    DEBUG_BREAK_IF(static_cast<uint32_t>(heapType) >= arrayCount(indirectHeap));
    auto &heap = indirectHeap[heapType];
    GraphicsAllocation *heapMemory = nullptr;

    if (heap)
        heapMemory = heap->getGraphicsAllocation();

    if (heap && heap->getAvailableSpace() < minRequiredSize && heapMemory) {
        internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapMemory), REUSABLE_ALLOCATION);
        heapMemory = nullptr;
    }

    if (!heapMemory) {
        allocateHeapMemory(heapType, minRequiredSize, heap);
    }

    return *heap;
}

void CommandStreamReceiver::allocateHeapMemory(IndirectHeap::Type heapType,
                                               size_t minRequiredSize, IndirectHeap *&indirectHeap) {
    size_t reservedSize = 0;
    auto finalHeapSize = defaultHeapSize;
    if (IndirectHeap::SURFACE_STATE == heapType) {
        finalHeapSize = defaultSshSize;
    }
    bool requireInternalHeap = IndirectHeap::INDIRECT_OBJECT == heapType ? true : false;

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        requireInternalHeap = false;
    }

    minRequiredSize += reservedSize;

    finalHeapSize = alignUp(std::max(finalHeapSize, minRequiredSize), MemoryConstants::pageSize);

    auto heapMemory = internalAllocationStorage->obtainReusableAllocation(finalHeapSize, requireInternalHeap).release();

    if (!heapMemory) {
        if (requireInternalHeap) {
            heapMemory = getMemoryManager()->allocate32BitGraphicsMemory(finalHeapSize, nullptr, AllocationOrigin::INTERNAL_ALLOCATION);
        } else {
            heapMemory = getMemoryManager()->allocateGraphicsMemory(finalHeapSize);
        }
    } else {
        finalHeapSize = std::max(heapMemory->getUnderlyingBufferSize(), finalHeapSize);
    }

    heapMemory->setAllocationType(GraphicsAllocation::AllocationType::LINEAR_STREAM);

    if (IndirectHeap::SURFACE_STATE == heapType) {
        DEBUG_BREAK_IF(minRequiredSize > defaultSshSize - MemoryConstants::pageSize);
        finalHeapSize = defaultSshSize - MemoryConstants::pageSize;
    }

    if (indirectHeap) {
        indirectHeap->replaceBuffer(heapMemory->getUnderlyingBuffer(), finalHeapSize);
        indirectHeap->replaceGraphicsAllocation(heapMemory);
    } else {
        indirectHeap = new IndirectHeap(heapMemory, requireInternalHeap);
        indirectHeap->overrideMaxSize(finalHeapSize);
    }
}

void CommandStreamReceiver::releaseIndirectHeap(IndirectHeap::Type heapType) {
    DEBUG_BREAK_IF(static_cast<uint32_t>(heapType) >= arrayCount(indirectHeap));
    auto &heap = indirectHeap[heapType];

    if (heap) {
        auto heapMemory = heap->getGraphicsAllocation();
        if (heapMemory != nullptr)
            internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapMemory), REUSABLE_ALLOCATION);
        heap->replaceBuffer(nullptr, 0);
        heap->replaceGraphicsAllocation(nullptr);
    }
}

void CommandStreamReceiver::setExperimentalCmdBuffer(std::unique_ptr<ExperimentalCommandBuffer> &&cmdBuffer) {
    experimentalCmdBuffer = std::move(cmdBuffer);
}

bool CommandStreamReceiver::initializeTagAllocation() {
    auto tagAllocation = getMemoryManager()->allocateGraphicsMemory(sizeof(uint32_t));
    if (!tagAllocation) {
        return false;
    }

    this->setTagAllocation(tagAllocation);
    *this->tagAddress = DebugManager.flags.EnableNullHardware.get() ? -1 : initialHardwareTag;

    return true;
}

std::unique_lock<CommandStreamReceiver::MutexType> CommandStreamReceiver::obtainUniqueOwnership() {
    return std::unique_lock<CommandStreamReceiver::MutexType>(this->ownershipMutex);
}
AllocationsList &CommandStreamReceiver::getTemporaryAllocations() { return internalAllocationStorage->getTemporaryAllocations(); }
AllocationsList &CommandStreamReceiver::getAllocationsForReuse() { return internalAllocationStorage->getAllocationsForReuse(); }

bool CommandStreamReceiver::createAllocationForHostSurface(HostPtrSurface &surface, Device &device, bool requiresL3Flush) {
    auto memoryManager = getMemoryManager();
    GraphicsAllocation *allocation = nullptr;
    allocation = memoryManager->allocateGraphicsMemoryForHostPtr(surface.getSurfaceSize(), surface.getMemoryPointer(), device.isFullRangeSvm(), requiresL3Flush);
    if (allocation == nullptr && surface.peekIsPtrCopyAllowed()) {
        // Try with no host pointer allocation and copy
        allocation = memoryManager->allocateGraphicsMemory(surface.getSurfaceSize(), MemoryConstants::pageSize, false, false);

        if (allocation) {
            memcpy_s(allocation->getUnderlyingBuffer(), allocation->getUnderlyingBufferSize(), surface.getMemoryPointer(), surface.getSurfaceSize());
        }
    }
    if (allocation == nullptr) {
        return false;
    }
    allocation->updateTaskCount(Event::eventNotReady, deviceIndex);
    surface.setAllocation(allocation);
    internalAllocationStorage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);
    return true;
}

} // namespace OCLRT

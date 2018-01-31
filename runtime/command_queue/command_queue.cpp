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

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/event/event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/string.h"
#include "CL/cl_ext.h"
#include "runtime/utilities/api_intercept.h"
#include "runtime/helpers/convert_color.h"
#include "runtime/helpers/queue_helpers.h"
#include <map>

namespace OCLRT {

// Global table of create functions
CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE] = {};

CommandQueue *CommandQueue::create(Context *context,
                                   Device *device,
                                   const cl_queue_properties *properties,
                                   cl_int &retVal) {
    retVal = CL_SUCCESS;

    auto funcCreate = commandQueueFactory[device->getRenderCoreFamily()];
    DEBUG_BREAK_IF(nullptr == funcCreate);

    return funcCreate(context, device, properties);
}

CommandQueue::CommandQueue() : CommandQueue(nullptr, nullptr, 0) {
}

CommandQueue::CommandQueue(Context *context,
                           Device *deviceId,
                           const cl_queue_properties *properties) : taskCount(0),
                                                                    taskLevel(0),
                                                                    virtualEvent(nullptr),
                                                                    context(context),
                                                                    device(deviceId),
                                                                    priority(QueuePriority::MEDIUM),
                                                                    perfCountersEnabled(false),
                                                                    perfCountersConfig(UINT32_MAX),
                                                                    perfCountersUserRegistersNumber(0),
                                                                    perfConfigurationData(nullptr),
                                                                    perfCountersRegsCfgHandle(0),
                                                                    perfCountersRegsCfgPending(false),
                                                                    commandStream(nullptr) {
    if (context) {
        context->incRefInternal();
    }
    for (int i = 0; i < NUM_HEAPS; ++i) {
        indirectHeap[i] = nullptr;
    }
    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    flushStamp.reset(new FlushStampTracker(true));
}

CommandQueue::~CommandQueue() {
    if (virtualEvent) {
        UNRECOVERABLE_IF(this->virtualEvent->getCommandQueue() != this && this->virtualEvent->getCommandQueue() != nullptr);
        virtualEvent->setCurrentCmdQVirtualEvent(false);
        virtualEvent->decRefInternal();
    }

    if (device) {
        auto memoryManager = device->getMemoryManager();
        DEBUG_BREAK_IF(nullptr == memoryManager);

        if (commandStream && commandStream->getGraphicsAllocation()) {
            memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream->getGraphicsAllocation()), REUSABLE_ALLOCATION);
            commandStream->replaceGraphicsAllocation(nullptr);
        }
        delete commandStream;

        for (int i = 0; i < NUM_HEAPS; ++i) {
            if (indirectHeap[i] != nullptr) {
                auto allocation = indirectHeap[i]->getGraphicsAllocation();
                if (allocation != nullptr) {
                    memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);
                }
                delete indirectHeap[i];
            }
        }
        if (perfConfigurationData) {
            delete perfConfigurationData;
        }
        if (this->perfCountersEnabled) {
            device->getPerformanceCounters()->shutdown();
        }
    }

    //for normal queue, decrement ref count on context
    //special queue is owned by context so ref count doesn't have to be decremented
    if (context && !isSpecialCommandQueue) {
        context->decRefInternal();
    }
}

uint32_t CommandQueue::getHwTag() const {
    uint32_t tag = *getHwTagAddress();
    return tag;
}

volatile uint32_t *CommandQueue::getHwTagAddress() const {
    DEBUG_BREAK_IF(!this->device);
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto tag_address = commandStreamReceiver.getTagAddress();
    commandStreamReceiver.makeCoherent((void *)tag_address, sizeof(tag_address));
    return tag_address;
}

bool CommandQueue::isCompleted(uint32_t taskCount) const {
    uint32_t tag = getHwTag();
    DEBUG_BREAK_IF(tag == Event::eventNotReady);
    return tag >= taskCount;
}

void CommandQueue::waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait) {
    WAIT_ENTER()

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Waiting for taskCount:", taskCountToWait);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", getHwTag());

    device->getCommandStreamReceiver().waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait);

    DEBUG_BREAK_IF(getHwTag() < taskCountToWait);
    latestTaskCountWaited = taskCountToWait;
    WAIT_LEAVE()
}

bool CommandQueue::isQueueBlocked() {
    TakeOwnershipWrapper<CommandQueue> takeOwnershipWrapper(*this);
    //check if we have user event and if so, if it is in blocked state.
    if (this->virtualEvent) {
        if (this->virtualEvent->peekExecutionStatus() <= CL_COMPLETE) {
            UNRECOVERABLE_IF(this->virtualEvent == nullptr);

            if (this->virtualEvent->isStatusCompletedByTermination() == false) {
                taskCount = this->virtualEvent->peekTaskCount();
                flushStamp->setStamp(this->virtualEvent->flushStamp->peekStamp());
                taskLevel = this->virtualEvent->taskLevel;
                // If this isn't an OOQ, update the taskLevel for the queue
                if (!isOOQEnabled()) {
                    taskLevel++;
                }
            } else {
                //at this point we may reset queue TaskCount, since all command previous to this were aborted
                taskCount = 0;
                flushStamp->setStamp(0);
                taskLevel = getDevice().getCommandStreamReceiver().peekTaskLevel();
            }

            DebugManager.log(DebugManager.flags.EventsDebugEnable.get(), "isQueueBlocked taskLevel change from", taskLevel, "to new from virtualEvent", this->virtualEvent, "new tasklevel", this->virtualEvent->taskLevel.load());

            //close the access to virtual event, driver added only 1 ref count.
            this->virtualEvent->decRefInternal();
            this->virtualEvent = nullptr;
            return false;
        }
        return true;
    }
    return false;
}

cl_int CommandQueue::getCommandQueueInfo(cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    return getQueueInfo<CommandQueue>(this, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

uint32_t CommandQueue::getTaskLevelFromWaitList(uint32_t taskLevel,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList) {
    for (auto iEvent = 0u; iEvent < numEventsInWaitList; ++iEvent) {
        auto pEvent = (Event *)(eventWaitList[iEvent]);
        uint32_t eventTaskLevel = pEvent->taskLevel;
        taskLevel = std::max(taskLevel, eventTaskLevel);
    }
    return taskLevel;
}

size_t CommandQueue::getInstructionHeapReservedBlockSize() const {
    return alignUp(device->getCommandStreamReceiver().getInstructionHeapCmdStreamReceiverReservedSize(),
                   MemoryConstants::cacheLineSize);
}

IndirectHeap &CommandQueue::getIndirectHeap(IndirectHeap::Type heapType,
                                            size_t minRequiredSize) {
    DEBUG_BREAK_IF(static_cast<uint32_t>(heapType) >= ARRAY_COUNT(indirectHeap));
    auto &heap = indirectHeap[heapType];
    GraphicsAllocation *heapMemory = nullptr;

    DEBUG_BREAK_IF(nullptr == device);
    auto memoryManager = device->getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    if (heap)
        heapMemory = heap->getGraphicsAllocation();

    if (heap && heap->getAvailableSpace() < minRequiredSize && heapMemory) {
        memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapMemory), REUSABLE_ALLOCATION);
        heapMemory = nullptr;
    }

    if (!heapMemory) {
        size_t reservedSize = 0;
        if (heapType == IndirectHeap::INSTRUCTION) {
            reservedSize = getInstructionHeapReservedBlockSize();
        }
        auto finalHeapSize = defaultHeapSize;
        minRequiredSize += reservedSize;

        finalHeapSize = alignUp(std::max(finalHeapSize, minRequiredSize), MemoryConstants::pageSize);

        heapMemory = memoryManager->obtainReusableAllocation(finalHeapSize).release();

        if (!heapMemory) {
            heapMemory = memoryManager->allocateGraphicsMemory(finalHeapSize, MemoryConstants::pageSize);
        } else {
            finalHeapSize = std::max(heapMemory->getUnderlyingBufferSize(), finalHeapSize);
        }

        if (IndirectHeap::SURFACE_STATE == heapType) {
            DEBUG_BREAK_IF(minRequiredSize > maxSshSize);
            finalHeapSize = maxSshSize;
        }

        if (heap) {
            heap->replaceBuffer(heapMemory->getUnderlyingBuffer(), finalHeapSize);
            heap->replaceGraphicsAllocation(heapMemory);
        } else {
            heap = new IndirectHeap(heapMemory);
            heap->overrideMaxSize(finalHeapSize);
        }

        if (heapType == IndirectHeap::INSTRUCTION) {
            device->getCommandStreamReceiver().initializeInstructionHeapCmdStreamReceiverReservedBlock(*heap);
            heap->align(MemoryConstants::cacheLineSize);
        }
    }

    return *heap;
}

void CommandQueue::releaseIndirectHeap(IndirectHeap::Type heapType) {
    DEBUG_BREAK_IF(static_cast<uint32_t>(heapType) >= ARRAY_COUNT(indirectHeap));
    auto &heap = indirectHeap[heapType];

    DEBUG_BREAK_IF(nullptr == device);
    auto memoryManager = device->getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    if (heap) {
        auto heapMemory = heap->getGraphicsAllocation();
        if (heapMemory != nullptr)
            memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapMemory), REUSABLE_ALLOCATION);
        heap->replaceBuffer(nullptr, 0);
        heap->replaceGraphicsAllocation(nullptr);
    }
}

LinearStream &CommandQueue::getCS(size_t minRequiredSize) {
    DEBUG_BREAK_IF(nullptr == device);
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto memoryManager = commandStreamReceiver.getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    if (!commandStream) {
        commandStream = new LinearStream(nullptr);
    }

    // Make sure we have enough room for any CSR additions
    minRequiredSize += CSRequirements::minCommandQueueCommandStreamSize;

    if (commandStream->getAvailableSpace() < minRequiredSize) {
        // If not, allocate a new block. allocate full pages
        minRequiredSize = alignUp(minRequiredSize, MemoryConstants::pageSize);

        auto requiredSize = minRequiredSize + CSRequirements::csOverfetchSize;

        GraphicsAllocation *allocation = memoryManager->obtainReusableAllocation(requiredSize).release();

        if (!allocation) {
            allocation = memoryManager->allocateGraphicsMemory(requiredSize, MemoryConstants::pageSize);
        }

        // Deallocate the old block, if not null
        auto oldAllocation = commandStream->getGraphicsAllocation();

        if (oldAllocation) {
            memoryManager->storeAllocation(std::unique_ptr<GraphicsAllocation>(oldAllocation), REUSABLE_ALLOCATION);
        }
        commandStream->replaceBuffer(allocation->getUnderlyingBuffer(), minRequiredSize - CSRequirements::minCommandQueueCommandStreamSize);
        commandStream->replaceGraphicsAllocation(allocation);
    }

    return *commandStream;
}

cl_int CommandQueue::enqueueAcquireSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        memObject->peekSharingHandler()->acquire(memObject);
        memObject->acquireCount++;
    }
    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }

    return status;
}

cl_int CommandQueue::enqueueReleaseSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        memObject->peekSharingHandler()->release(memObject);
        DEBUG_BREAK_IF(memObject->acquireCount <= 0);
        memObject->acquireCount--;
    }
    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }
    return status;
}

void CommandQueue::updateFromCompletionStamp(const CompletionStamp &completionStamp) {
    DEBUG_BREAK_IF(this->taskLevel > completionStamp.taskLevel);
    DEBUG_BREAK_IF(this->taskCount > completionStamp.taskCount);
    if (completionStamp.taskCount != Event::eventNotReady) {
        taskCount = completionStamp.taskCount;
    }
    flushStamp->setStamp(completionStamp.flushStamp);
    this->taskLevel = completionStamp.taskLevel;
}

void CommandQueue::flushWaitList(
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    bool ndRangeKernel) {

    bool isQBlocked = false;

    //as long as queue is blocked we need to stall.
    if (!isOOQEnabled()) {
        while ((isQBlocked = isQueueBlocked()))
            ;
    }

    TakeOwnershipWrapper<Device> deviceOwnership(*device);
    device->getCommandStreamReceiver().flushBatchedSubmissions();

    if (!isQBlocked) {
        auto taskLevel = getTaskLevelFromWaitList(this->taskLevel, numEventsInWaitList, eventWaitList);
        auto &commandStream = getCS();
        auto &commandStreamReceiver = device->getCommandStreamReceiver();

        bool flushTask = false;

        for (auto eventId = 0u; eventId < numEventsInWaitList; eventId++) {
            Event *event = (Event *)eventWaitList[eventId];
            if (event->peekTaskCount() > commandStreamReceiver.peekLatestSentTaskCount()) {
                flushTask = true;
                break;
            }
        }

        if (flushTask) {
            DispatchFlags dispatchFlags;
            dispatchFlags.GSBA32BitRequired = ndRangeKernel;
            dispatchFlags.lowPriority = priority == QueuePriority::LOW;
            dispatchFlags.implicitFlush = true;
            dispatchFlags.preemptionMode = PreemptionHelper::taskPreemptionMode(*device, nullptr);

            DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

            commandStreamReceiver.flushTask(
                commandStream,
                commandStream.getUsed(),
                getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0),
                getIndirectHeap(IndirectHeap::INSTRUCTION, 0),
                getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0),
                getIndirectHeap(IndirectHeap::SURFACE_STATE, 0),
                taskLevel + 1,
                dispatchFlags);
        }
    }
}

bool CommandQueue::setPerfCountersEnabled(bool perfCountersEnabled, cl_uint configuration) {
    DEBUG_BREAK_IF(device == nullptr);
    if (perfCountersEnabled == this->perfCountersEnabled) {
        return true;
    }
    auto perfCounters = device->getPerformanceCounters();
    if (perfCountersEnabled) {
        perfCounters->enable();
        if (!perfCounters->isAvailable()) {
            perfCounters->shutdown();
            return false;
        }
        perfConfigurationData = perfCounters->getPmRegsCfg(configuration);
        if (perfConfigurationData == nullptr) {
            perfCounters->shutdown();
            return false;
        }
        InstrReadRegsCfg *pUserCounters = &perfConfigurationData->ReadRegs;
        for (uint32_t i = 0; i < pUserCounters->RegsCount; ++i) {
            perfCountersUserRegistersNumber++;
            if (pUserCounters->Reg[i].BitSize > 32) {
                perfCountersUserRegistersNumber++;
            }
        }
    } else {
        if (perfCounters->isAvailable()) {
            perfCounters->shutdown();
        }
    }
    this->perfCountersConfig = configuration;
    this->perfCountersEnabled = perfCountersEnabled;

    return true;
}

PerformanceCounters *CommandQueue::getPerfCounters() {
    return device->getPerformanceCounters();
}

bool CommandQueue::sendPerfCountersConfig() {
    return getPerfCounters()->sendPmRegsCfgCommands(perfConfigurationData, &perfCountersRegsCfgHandle, &perfCountersRegsCfgPending);
}

} // namespace OCLRT

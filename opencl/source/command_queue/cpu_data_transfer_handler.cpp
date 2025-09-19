/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/source/utilities/logger.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {
void cachelineFlushMemory(char *ptr, size_t size) {
    const auto lastPtr = ptr + size;
    while (ptr < lastPtr) {
        CpuIntrinsics::clFlushOpt(ptr);
        ptr += MemoryConstants::cacheLineSize;
    }
    CpuIntrinsics::sfence();
}

void *CommandQueue::cpuDataTransferHandler(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &retVal) {
    MapInfo unmapInfo;
    Event *outEventObj = nullptr;
    void *returnPtr = nullptr;
    EventBuilder eventBuilder;
    bool eventCompleted = false;
    bool mapOperation = transferProperties.cmdType == CL_COMMAND_MAP_BUFFER || transferProperties.cmdType == CL_COMMAND_MAP_IMAGE;

    ErrorCodeHelper err(&retVal, CL_SUCCESS);

    if (mapOperation) {
        returnPtr = ptrOffset(transferProperties.memObj->getCpuAddressForMapping(),
                              transferProperties.memObj->calculateOffsetForMapping(transferProperties.offset) + transferProperties.mipPtrOffset);

        if (!transferProperties.memObj->addMappedPtr(returnPtr, transferProperties.memObj->calculateMappedPtrLength(transferProperties.size),
                                                     transferProperties.mapFlags, transferProperties.size, transferProperties.offset, transferProperties.mipLevel, nullptr)) {
            err.set(CL_INVALID_OPERATION);
            return nullptr;
        }
    } else if (transferProperties.cmdType == CL_COMMAND_UNMAP_MEM_OBJECT) {
        if (!transferProperties.memObj->findMappedPtr(transferProperties.ptr, unmapInfo)) {
            err.set(CL_INVALID_VALUE);
            return nullptr;
        }
        transferProperties.memObj->removeMappedPtr(unmapInfo.ptr);
    }
    auto blockQueue = false;
    TaskCountType taskLevel = 0u;
    TakeOwnershipWrapper<CommandQueue> queueOwnership(*this);
    auto commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
    obtainTaskLevelAndBlockedStatus(taskLevel, eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, blockQueue, transferProperties.cmdType);
    bool isMarkerRequiredForEventSignal = !blockQueue &&
                                          !transferProperties.blocking &&
                                          !transferProperties.finishRequired &&
                                          !isOOQEnabled() &&
                                          eventsRequest.outEvent != nullptr;

    if (eventsRequest.outEvent && !isMarkerRequiredForEventSignal) {
        eventBuilder.create<Event>(this, transferProperties.cmdType, CompletionStamp::notReady, CompletionStamp::notReady);
        outEventObj = eventBuilder.getEvent();
        outEventObj->setQueueTimeStamp();
        outEventObj->setCPUProfilingPath(true);
        *eventsRequest.outEvent = outEventObj;
    }

    DBG_LOG(LogTaskCounts, __FUNCTION__, "taskLevel", taskLevel);

    if (outEventObj) {
        outEventObj->taskLevel = taskLevel;
    }

    if (blockQueue &&
        (transferProperties.cmdType == CL_COMMAND_MAP_BUFFER ||
         transferProperties.cmdType == CL_COMMAND_MAP_IMAGE ||
         transferProperties.cmdType == CL_COMMAND_UNMAP_MEM_OBJECT)) {
        // Pass size and offset only. Unblocked command will call transferData(size, offset) method
        enqueueBlockedMapUnmapOperation(eventsRequest.eventWaitList,
                                        static_cast<size_t>(eventsRequest.numEventsInWaitList),
                                        mapOperation ? MapOperationType::map : MapOperationType::unmap,
                                        transferProperties.memObj,
                                        mapOperation ? transferProperties.size : unmapInfo.size,
                                        mapOperation ? transferProperties.offset : unmapInfo.offset,
                                        mapOperation ? transferProperties.mapFlags == CL_MAP_READ : unmapInfo.readOnly,
                                        eventBuilder);
    }
    if (!isMarkerRequiredForEventSignal) {
        commandStreamReceiverOwnership.unlock();
        queueOwnership.unlock();
    }

    // read/write buffers are always blocking
    if (!blockQueue || transferProperties.blocking) {
        err.set(Event::waitForEvents(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList));
        bool modifySimulationFlags = false;

        if (outEventObj) {
            outEventObj->setSubmitTimeStamp();
        }
        // wait for the completeness of previous commands
        if (transferProperties.finishRequired) {
            auto ret = finish(true);

            if (ret != CL_SUCCESS) {
                err.set(ret);
                return nullptr;
            }
            eventCompleted = true;
        }

        if (outEventObj) {
            outEventObj->setStartTimeStamp();
        }

        UNRECOVERABLE_IF((transferProperties.memObj->isMemObjZeroCopy() == false) && isMipMapped(transferProperties.memObj));
        switch (transferProperties.cmdType) {
        case CL_COMMAND_MAP_BUFFER:
            if (!transferProperties.memObj->isMemObjZeroCopy()) {
                if (transferProperties.mapFlags != CL_MAP_WRITE_INVALIDATE_REGION) {
                    transferProperties.memObj->transferDataToHostPtr(transferProperties.size, transferProperties.offset);
                }
                eventCompleted = true;
            } else if (debugManager.flags.AllowZeroCopyWithoutCoherency.get() == 1) {
                cachelineFlushMemory(static_cast<char *>(transferProperties.getCpuPtrForReadWrite()), transferProperties.size[0]);
            }
            break;
        case CL_COMMAND_MAP_IMAGE:
            if (!transferProperties.memObj->isMemObjZeroCopy()) {
                if (transferProperties.mapFlags != CL_MAP_WRITE_INVALIDATE_REGION) {
                    transferProperties.memObj->transferDataToHostPtr(transferProperties.size, transferProperties.offset);
                }
                eventCompleted = true;
            }
            break;
        case CL_COMMAND_UNMAP_MEM_OBJECT:
            if (!transferProperties.memObj->isMemObjZeroCopy()) {
                if (!unmapInfo.readOnly) {
                    transferProperties.memObj->transferDataFromHostPtr(unmapInfo.size, unmapInfo.offset);
                }
                eventCompleted = true;
            } else if (debugManager.flags.AllowZeroCopyWithoutCoherency.get() == 1) {
                cachelineFlushMemory(static_cast<char *>(transferProperties.getCpuPtrForReadWrite()), transferProperties.memObj->getSize());
            }
            if (!unmapInfo.readOnly) {
                modifySimulationFlags = true;
            }
            break;
        case CL_COMMAND_READ_BUFFER:
            memcpy_s(transferProperties.ptr, transferProperties.size[0], transferProperties.getCpuPtrForReadWrite(), transferProperties.size[0]);
            eventCompleted = true;
            break;
        case CL_COMMAND_WRITE_BUFFER:
            memcpy_s(transferProperties.getCpuPtrForReadWrite(), transferProperties.size[0], transferProperties.ptr, transferProperties.size[0]);
            eventCompleted = true;
            modifySimulationFlags = true;
            break;
        case CL_COMMAND_MARKER:
            break;
        default:
            err.set(CL_INVALID_OPERATION);
        }

        if (outEventObj) {
            outEventObj->setEndTimeStamp();
            outEventObj->updateTaskCount(this->taskCount, outEventObj->peekBcsTaskCountFromCommandQueue());
            outEventObj->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
            if (eventCompleted) {
                outEventObj->setStatus(CL_COMPLETE);
            } else {
                outEventObj->updateExecutionStatus();
            }
        } else if (isMarkerRequiredForEventSignal) {
            enqueueMarkerWithWaitList(0, nullptr, eventsRequest.outEvent);
            commandStreamReceiverOwnership.unlock();
            queueOwnership.unlock();
            outEventObj = castToObject<Event>(*eventsRequest.outEvent);
            outEventObj->setCmdType(transferProperties.cmdType);
        }
        if (modifySimulationFlags) {
            auto graphicsAllocation = transferProperties.memObj->getGraphicsAllocation(getDevice().getRootDeviceIndex());
            graphicsAllocation->setAubWritable(true, GraphicsAllocation::defaultBank);
            graphicsAllocation->setTbxWritable(true, GraphicsAllocation::defaultBank);
        }
    }

    if (context->isProvidingPerformanceHints()) {
        providePerformanceHint(transferProperties);
    }

    return returnPtr; // only map returns pointer
}

void CommandQueue::providePerformanceHint(TransferProperties &transferProperties) {
    switch (transferProperties.cmdType) {
    case CL_COMMAND_MAP_BUFFER:
    case CL_COMMAND_MAP_IMAGE:
        context->providePerformanceHintForMemoryTransfer(transferProperties.cmdType, !transferProperties.memObj->isMemObjZeroCopy(),
                                                         static_cast<cl_mem>(transferProperties.memObj));
        break;
    case CL_COMMAND_UNMAP_MEM_OBJECT:
        if (!transferProperties.memObj->isMemObjZeroCopy()) {
            context->providePerformanceHintForMemoryTransfer(transferProperties.cmdType, true,
                                                             transferProperties.ptr, static_cast<cl_mem>(transferProperties.memObj));
            break;
        }
        context->providePerformanceHintForMemoryTransfer(transferProperties.cmdType, false, transferProperties.ptr);
        break;
    case CL_COMMAND_READ_BUFFER:
    case CL_COMMAND_WRITE_BUFFER:
        context->providePerformanceHintForMemoryTransfer(transferProperties.cmdType, true,
                                                         static_cast<cl_mem>(transferProperties.memObj), transferProperties.ptr);
        break;
    }
}
} // namespace NEO

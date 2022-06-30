/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {
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

    if (eventsRequest.outEvent) {
        eventBuilder.create<Event>(this, transferProperties.cmdType, CompletionStamp::notReady, CompletionStamp::notReady);
        outEventObj = eventBuilder.getEvent();
        outEventObj->setQueueTimeStamp();
        outEventObj->setCPUProfilingPath(true);
        *eventsRequest.outEvent = outEventObj;
    }

    TakeOwnershipWrapper<CommandQueue> queueOwnership(*this);
    auto commandStreamReceieverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();

    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, blockQueue, transferProperties.cmdType);

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
                                        mapOperation ? MAP : UNMAP,
                                        transferProperties.memObj,
                                        mapOperation ? transferProperties.size : unmapInfo.size,
                                        mapOperation ? transferProperties.offset : unmapInfo.offset,
                                        mapOperation ? transferProperties.mapFlags == CL_MAP_READ : unmapInfo.readOnly,
                                        eventBuilder);
    }

    commandStreamReceieverOwnership.unlock();
    queueOwnership.unlock();

    // read/write buffers are always blocking
    if (!blockQueue || transferProperties.blocking) {
        err.set(Event::waitForEvents(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList));
        bool modifySimulationFlags = false;

        if (outEventObj) {
            outEventObj->setSubmitTimeStamp();
        }
        //wait for the completness of previous commands
        if (transferProperties.cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
            if (!transferProperties.memObj->isMemObjZeroCopy() || transferProperties.blocking) {
                finish();
                eventCompleted = true;
            }
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

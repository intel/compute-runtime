/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/event/event_builder.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"

namespace OCLRT {
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
                                                     transferProperties.mapFlags, transferProperties.size, transferProperties.offset, transferProperties.mipLevel)) {
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
        eventBuilder.create<Event>(this, transferProperties.cmdType, Event::eventNotReady, Event::eventNotReady);
        outEventObj = eventBuilder.getEvent();
        outEventObj->setQueueTimeStamp();
        outEventObj->setCPUProfilingPath(true);
        *eventsRequest.outEvent = outEventObj;
    }

    auto commandStreamReceieverOwnership = device->getCommandStreamReceiver().obtainUniqueOwnership();
    TakeOwnershipWrapper<CommandQueue> queueOwnership(*this);

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

    queueOwnership.unlock();
    commandStreamReceieverOwnership.unlock();

    // read/write buffers are always blocking
    if (!blockQueue || transferProperties.blocking) {
        err.set(Event::waitForEvents(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList));

        if (outEventObj) {
            outEventObj->setSubmitTimeStamp();
        }
        //wait for the completness of previous commands
        if (transferProperties.cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
            if (!transferProperties.memObj->isMemObjZeroCopy() || transferProperties.blocking) {
                finish(true);
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
                transferProperties.memObj->transferDataToHostPtr(transferProperties.size, transferProperties.offset);
                eventCompleted = true;
            }
            break;
        case CL_COMMAND_MAP_IMAGE:
            if (!transferProperties.memObj->isMemObjZeroCopy()) {
                transferProperties.memObj->transferDataToHostPtr(transferProperties.size, transferProperties.offset);
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
                auto graphicsAllocation = transferProperties.memObj->getGraphicsAllocation();
                graphicsAllocation->setAubWritable(true);
            }
            break;
        case CL_COMMAND_READ_BUFFER:
            memcpy_s(transferProperties.ptr, transferProperties.size[0], ptrOffset(transferProperties.memObj->getCpuAddressForMemoryTransfer(), transferProperties.offset[0]), transferProperties.size[0]);
            eventCompleted = true;
            break;
        case CL_COMMAND_WRITE_BUFFER:
            memcpy_s(ptrOffset(transferProperties.memObj->getCpuAddressForMemoryTransfer(), transferProperties.offset[0]), transferProperties.size[0], transferProperties.ptr, transferProperties.size[0]);
            eventCompleted = true;
            break;
        case CL_COMMAND_MARKER:
            break;
        default:
            err.set(CL_INVALID_OPERATION);
        }

        if (outEventObj) {
            outEventObj->setEndTimeStamp();
            outEventObj->updateTaskCount(this->taskCount);
            outEventObj->flushStamp->setStamp(this->flushStamp->peekStamp());
            if (eventCompleted) {
                outEventObj->setStatus(CL_COMPLETE);
            } else {
                outEventObj->updateExecutionStatus();
            }
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
        if (!transferProperties.memObj->isMemObjZeroCopy()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_MAP_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj));
            break;
        }
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_MAP_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj));
        break;
    case CL_COMMAND_MAP_IMAGE:
        if (!transferProperties.memObj->isMemObjZeroCopy()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_MAP_IMAGE_REQUIRES_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj));
            break;
        }
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_MAP_IMAGE_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj));
        break;
    case CL_COMMAND_UNMAP_MEM_OBJECT:
        if (!transferProperties.memObj->isMemObjZeroCopy()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_UNMAP_MEM_OBJ_REQUIRES_COPY_DATA, transferProperties.ptr, static_cast<cl_mem>(transferProperties.memObj));
            break;
        }
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_UNMAP_MEM_OBJ_DOESNT_REQUIRE_COPY_DATA, transferProperties.ptr);
        break;
    case CL_COMMAND_READ_BUFFER:
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj), transferProperties.ptr);
        break;
    case CL_COMMAND_WRITE_BUFFER:
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(transferProperties.memObj), transferProperties.ptr);
    }
}
} // namespace OCLRT

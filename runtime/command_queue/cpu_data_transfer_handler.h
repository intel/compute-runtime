/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/device/device.h"
#include "runtime/event/event_builder.h"

namespace OCLRT {
template <typename GfxFamily>
void *CommandQueueHw<GfxFamily>::cpuDataTransferHandler(MemObj *memObj,
                                                        cl_command_type cmdType,
                                                        cl_bool blocking,
                                                        size_t offset,
                                                        size_t size,
                                                        void *ptr,
                                                        cl_uint numEventsInWaitList,
                                                        const cl_event *eventWaitList,
                                                        cl_event *event,
                                                        cl_int &retVal) {
    EventBuilder eventBuilder;
    bool eventCompleted = false;
    ErrorCodeHelper err(&retVal, CL_SUCCESS);

    if (event) {
        eventBuilder.create<Event>(this, cmdType, Event::eventNotReady, Event::eventNotReady);
        eventBuilder.getEvent()->setQueueTimeStamp();
        eventBuilder.getEvent()->setCPUProfilingPath(true);
        *event = eventBuilder.getEvent();
    }

    TakeOwnershipWrapper<Device> deviceOwnership(*device);
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, numEventsInWaitList, eventWaitList, blockQueue, cmdType);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "taskLevel", taskLevel);

    if (event) {
        eventBuilder.getEvent()->taskLevel = taskLevel;
    }

    if (blockQueue &&
        (cmdType == CL_COMMAND_MAP_BUFFER || cmdType == CL_COMMAND_UNMAP_MEM_OBJECT)) {

        addMapUnmapToWaitlistEventsDependencies(eventWaitList,
                                                static_cast<size_t>(numEventsInWaitList),
                                                cmdType == CL_COMMAND_MAP_BUFFER ? MAP : UNMAP,
                                                memObj,
                                                eventBuilder);
    }

    queueOwnership.unlock();
    deviceOwnership.unlock();

    // read/write buffers are always blocking
    if (!blockQueue || blocking) {
        err.set(Event::waitForEvents(numEventsInWaitList, eventWaitList));

        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setSubmitTimeStamp();
        }
        //wait for the completness of previous commands
        if (cmdType != CL_COMMAND_UNMAP_MEM_OBJECT) {
            if (!memObj->isMemObjZeroCopy() || blocking) {
                finish(true);
                eventCompleted = true;
            }
        }

        auto bufferStorage = ptrOffset(memObj->getCpuAddressForMemoryTransfer(), offset);

        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setStartTimeStamp();
        }

        switch (cmdType) {
        case CL_COMMAND_MAP_BUFFER:
            if (!memObj->isMemObjZeroCopy()) {
                if (context->isProvidingPerformanceHints()) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_MAP_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(memObj));
                }
                memObj->transferDataToHostPtr();
                eventCompleted = true;
            } else {
                if (context->isProvidingPerformanceHints()) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_MAP_BUFFER_DOESNT_REQUIRE_COPY_DATA, static_cast<cl_mem>(memObj));
                }
            }
            break;
        case CL_COMMAND_UNMAP_MEM_OBJECT:
            if (!memObj->isMemObjZeroCopy()) {
                if (context->isProvidingPerformanceHints()) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_UNMAP_MEM_OBJ_REQUIRES_COPY_DATA, ptr, static_cast<cl_mem>(memObj));
                }
                memObj->transferDataFromHostPtrToMemoryStorage();
                eventCompleted = true;
            } else {
                if (context->isProvidingPerformanceHints()) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_UNMAP_MEM_OBJ_DOESNT_REQUIRE_COPY_DATA, ptr);
                }
            }
            break;
        case CL_COMMAND_READ_BUFFER:
            if (context->isProvidingPerformanceHints()) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(memObj), ptr);
            }
            memcpy_s(ptr, size, bufferStorage, size);
            eventCompleted = true;
            break;
        case CL_COMMAND_WRITE_BUFFER:
            if (context->isProvidingPerformanceHints()) {
                context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA, static_cast<cl_mem>(memObj), ptr);
            }
            memcpy_s(bufferStorage, size, ptr, size);
            eventCompleted = true;
            break;
        default:
            err.set(CL_INVALID_OPERATION);
        }

        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setEndTimeStamp();
            eventBuilder.getEvent()->updateTaskCount(this->taskCount);
            if (eventCompleted) {
                eventBuilder.getEvent()->setStatus(CL_COMPLETE);
            } else {
                eventBuilder.getEvent()->updateExecutionStatus();
            }
        }
    }

    if (cmdType == CL_COMMAND_MAP_BUFFER) {
        return memObj->setAndReturnMappedPtr(offset);
    }

    if (cmdType == CL_COMMAND_UNMAP_MEM_OBJECT) {
        err.set(ptr == memObj->getMappedPtr() ? CL_SUCCESS : CL_INVALID_VALUE);
    }

    return nullptr; // only map returns pointer
}
} // namespace OCLRT

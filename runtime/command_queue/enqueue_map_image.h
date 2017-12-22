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
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/image.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/helpers/get_info.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
void *CommandQueueHw<GfxFamily>::enqueueMapImage(cl_mem image, cl_bool blockingMap,
                                                 cl_map_flags mapFlags, const size_t *origin,
                                                 const size_t *region, size_t *imageRowPitch,
                                                 size_t *imageSlicePitch, cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList, cl_event *event,
                                                 cl_int &errcodeRet) {
    auto pImage = castToObject<Image>(image);
    void *ptrToReturn = nullptr;
    if (context->isProvidingPerformanceHints()) {
        if (pImage->isMemObjZeroCopy()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_ENQUEUE_MAP_IMAGE_DOESNT_REQUIRE_COPY_DATA, image);
        } else {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_ENQUEUE_MAP_IMAGE_REQUIRES_COPY_DATA, image);
        }
    }

    if (pImage->allowTiling() || pImage->peekSharingHandler()) {
        TakeOwnershipWrapper<Image> imageOwnership(*pImage);
        size_t slicePitch = pImage->getHostPtrSlicePitch();
        GetInfoHelper retSlice(imageSlicePitch, sizeof(size_t), nullptr);
        retSlice.set<size_t>(slicePitch);

        size_t rowPitch = pImage->getHostPtrRowPitch();
        GetInfoHelper retRowPitch(imageRowPitch, sizeof(size_t), nullptr);
        retRowPitch.set<size_t>(rowPitch);

        auto memoryManager = device->getMemoryManager();

        size_t Region[] = {region[0] ? region[0] : 1,
                           region[1] ? region[1] : 1,
                           region[2] ? region[2] : 1};

        if (pImage->getFlags() & CL_MEM_USE_HOST_PTR) {
            size_t offset =
                pImage->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                pImage->getImageDesc().image_row_pitch * origin[1] +
                pImage->getImageDesc().image_slice_pitch * origin[2];
            auto mappedPtr = ptrOffset(pImage->getHostPtr(), offset);
            pImage->setMappedPtr(mappedPtr);
        } else if (!pImage->getAllocatedMappedPtr()) {
            auto memory = memoryManager->allocateSystemMemory(pImage->getSize(), MemoryConstants::pageSize);
            pImage->setAllocatedMappedPtr(memory);
        }

        errcodeRet = enqueueReadImage(pImage, blockingMap, origin, Region, rowPitch, slicePitch, pImage->getMappedPtr(),
                                      numEventsInWaitList, eventWaitList, event);

        if (errcodeRet == CL_SUCCESS) {
            pImage->setMappedOrigin((size_t *)origin);
            pImage->setMappedRegion((size_t *)region);
            return pImage->getMappedPtr();
        } else {
            return nullptr;
        }
    }

    EventBuilder eventBuilder;
    TakeOwnershipWrapper<Device> deviceOwnership(*device);
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);
    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, numEventsInWaitList, eventWaitList, blockQueue, CL_COMMAND_MAP_IMAGE);

    if (event) {
        eventBuilder.create<Event>(this, CL_COMMAND_MAP_IMAGE, taskLevel, Event::eventNotReady);
        *event = eventBuilder.getEvent();
        eventBuilder.getEvent()->setQueueTimeStamp();
    }

    if (blockQueue) {
        addMapUnmapToWaitlistEventsDependencies(eventWaitList,
                                                static_cast<size_t>(numEventsInWaitList),
                                                MAP,
                                                pImage,
                                                eventBuilder);
    }

    queueOwnership.unlock();
    deviceOwnership.unlock();

    if (blockingMap && blockQueue) {
        errcodeRet = this->virtualEvent->waitForEvents(numEventsInWaitList, eventWaitList);
    }

    if (!blockQueue) {
        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setSubmitTimeStamp();
        }

        finish(true);
        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setStartTimeStamp();
        }

        if (!pImage->isMemObjZeroCopy()) {
            pImage->transferDataToHostPtr();
        }
        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->setStatus(CL_COMPLETE);
            eventBuilder.getEvent()->updateTaskCount(this->taskCount);
            eventBuilder.getEvent()->setEndTimeStamp();
        }
    }

    if (imageSlicePitch) {
        if (pImage->isMemObjZeroCopy()) {
            *imageSlicePitch = pImage->getImageDesc().image_slice_pitch;
        } else {
            *imageSlicePitch = pImage->getHostPtrSlicePitch();
        }
    }

    if (imageRowPitch) {
        if (pImage->isMemObjZeroCopy()) {
            *imageRowPitch = pImage->getImageDesc().image_row_pitch;
        } else {
            *imageRowPitch = pImage->getHostPtrRowPitch();
        }
    }

    size_t offset =
        pImage->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
        pImage->getImageDesc().image_row_pitch * origin[1] +
        pImage->getImageDesc().image_slice_pitch * origin[2];
    if (pImage->isMemObjZeroCopy()) {
        ptrToReturn = ptrOffset(pImage->getCpuAddress(), offset);
    } else {
        ptrToReturn = ptrOffset(pImage->getHostPtr(), offset);
    }
    errcodeRet = CL_SUCCESS;
    pImage->setMappedPtr(ptrToReturn);
    return ptrToReturn;
}
} // namespace OCLRT

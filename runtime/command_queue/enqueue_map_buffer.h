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

namespace OCLRT {
template <typename GfxFamily>
void *CommandQueueHw<GfxFamily>::enqueueMapBuffer(Buffer *buffer, cl_bool blockingMap, cl_map_flags mapFlags,
                                                  size_t offset, size_t size, cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList, cl_event *event, cl_int &errcodeRet) {
    if (buffer->peekSharingHandler()) {
        return enqueueMapSharedBuffer(buffer, blockingMap, mapFlags, offset, size,
                                      numEventsInWaitList, eventWaitList, event, errcodeRet);
    }
    return cpuDataTransferHandler(reinterpret_cast<MemObj *>(buffer), CL_COMMAND_MAP_BUFFER,
                                  blockingMap, offset, size, nullptr,
                                  numEventsInWaitList, eventWaitList, event, errcodeRet);
}

template <typename GfxFamily>
void *CommandQueueHw<GfxFamily>::enqueueMapSharedBuffer(Buffer *buffer, cl_bool blockingMap, cl_map_flags mapFlags,
                                                        size_t offset, size_t size, cl_uint numEventsInWaitList,
                                                        const cl_event *eventWaitList, cl_event *event, cl_int &errcodeRet) {
    auto memoryManager = device->getMemoryManager();
    if (!buffer->getMappedPtr()) {
        auto memory = memoryManager->allocateSystemMemory(buffer->getGraphicsAllocation()->getUnderlyingBufferSize(), 0);
        buffer->setAllocatedMappedPtr(memory);
    }

    auto returnPtr = ptrOffset(buffer->getMappedPtr(), offset);
    errcodeRet = enqueueReadBuffer(buffer, blockingMap, offset, size, returnPtr,
                                   numEventsInWaitList, eventWaitList, event);
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }
    buffer->incMapCount();
    buffer->setMappedSize(size);
    buffer->setMappedOffset(offset);
    return returnPtr;
}
} // namespace OCLRT

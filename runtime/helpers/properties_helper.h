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

#pragma once

#include "runtime/api/cl_types.h"
#include <array>

namespace OCLRT {
class MemObj;

enum class QueueThrottle {
    LOW,
    MEDIUM,
    HIGH
};

struct EventsRequest {
    EventsRequest() = delete;

    EventsRequest(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *outEvent)
        : numEventsInWaitList(numEventsInWaitList), eventWaitList(eventWaitList), outEvent(outEvent){};

    cl_uint numEventsInWaitList;
    const cl_event *eventWaitList;
    cl_event *outEvent;
};

struct TransferProperties {
    TransferProperties() = delete;

    TransferProperties(MemObj *memObj, cl_command_type cmdType, bool blocking, size_t *offsetPtr, size_t *sizePtr,
                       void *ptr, size_t *retRowPitchPtr, size_t *retSlicePitchPtr)
        : memObj(memObj), cmdType(cmdType), blocking(blocking), offsetPtr(offsetPtr), sizePtr(sizePtr),
          ptr(ptr), retRowPitchPtr(retRowPitchPtr), retSlicePitchPtr(retSlicePitchPtr){};

    MemObj *memObj;
    cl_command_type cmdType;
    bool blocking;
    size_t *offsetPtr;
    size_t *sizePtr;
    void *ptr;
    size_t *retRowPitchPtr;
    size_t *retSlicePitchPtr;
};

struct MapInfo {
    using SizeArray = std::array<size_t, 3>;
    using OffsetArray = std::array<size_t, 3>;

    void *ptr = nullptr;
    SizeArray size = {{0, 0, 0}};
    OffsetArray offset = {{0, 0, 0}};
};

} // namespace OCLRT

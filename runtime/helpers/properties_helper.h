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
#include <unordered_set>

namespace OCLRT {
class MemObj;
class Buffer;

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

using MemObjSizeArray = std::array<size_t, 3>;
using MemObjOffsetArray = std::array<size_t, 3>;
using BuffersForAuxTranslation = std::unordered_set<Buffer *>;

enum class AuxTranslationDirection {
    None,
    AuxToNonAux,
    NonAuxToAux
};

struct TransferProperties {
    TransferProperties() = delete;

    TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking, size_t *offsetPtr, size_t *sizePtr,
                       void *ptr);

    MemObj *memObj = nullptr;
    cl_command_type cmdType = 0;
    cl_map_flags mapFlags = 0;
    bool blocking = false;
    MemObjOffsetArray offset = {};
    MemObjSizeArray size = {};
    void *ptr = nullptr;
    uint32_t mipLevel = 0;
    uint32_t mipPtrOffset = 0;
};

struct MapInfo {
    MapInfo() = default;
    MapInfo(void *ptr, size_t ptrLength, MemObjSizeArray size, MemObjOffsetArray offset, uint32_t mipLevel)
        : ptr(ptr), ptrLength(ptrLength), size(size), offset(offset), mipLevel(mipLevel) {
    }

    void *ptr = nullptr;
    size_t ptrLength = 0;
    MemObjSizeArray size = {};
    MemObjOffsetArray offset = {};
    bool readOnly = false;
    uint32_t mipLevel = 0;
};

class NonCopyableOrMovableClass {
  public:
    NonCopyableOrMovableClass() = default;
    NonCopyableOrMovableClass(const NonCopyableOrMovableClass &) = delete;
    NonCopyableOrMovableClass &operator=(const NonCopyableOrMovableClass &) = delete;

    NonCopyableOrMovableClass(NonCopyableOrMovableClass &&) = delete;
    NonCopyableOrMovableClass &operator=(NonCopyableOrMovableClass &&) = delete;
};
} // namespace OCLRT

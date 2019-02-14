/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/api/cl_types.h"
#include <array>
#include <unordered_set>

namespace OCLRT {
class MemObj;
class Buffer;

enum QueueThrottle : uint32_t {
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
using MemObjsForAuxTranslation = std::unordered_set<MemObj *>;

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

    void *lockedPtr = nullptr;
    void *getCpuPtrForReadWrite();
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

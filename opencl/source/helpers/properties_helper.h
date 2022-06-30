/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/command_stream/queue_throttle.h"

#include "opencl/source/api/cl_types.h"

#include <array>

namespace NEO {
class MemObj;
class Buffer;
class GraphicsAllocation;

struct EventsRequest {
    EventsRequest() = delete;

    EventsRequest(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *outEvent)
        : numEventsInWaitList(numEventsInWaitList), eventWaitList(eventWaitList), outEvent(outEvent) {}

    void fillCsrDependenciesForTimestampPacketContainer(CsrDependencies &csrDeps, CommandStreamReceiver &currentCsr, CsrDependencies::DependenciesType depsType) const;
    void fillCsrDependenciesForTaskCountContainer(CsrDependencies &csrDeps, CommandStreamReceiver &currentCsr) const;
    void setupBcsCsrForOutputEvent(CommandStreamReceiver &bcsCsr) const;

    cl_uint numEventsInWaitList;
    const cl_event *eventWaitList;
    cl_event *outEvent;
};

using MemObjSizeArray = std::array<size_t, 3>;
using MemObjOffsetArray = std::array<size_t, 3>;

struct TransferProperties {
    TransferProperties() = delete;

    TransferProperties(MemObj *memObj, cl_command_type cmdType, cl_map_flags mapFlags, bool blocking, size_t *offsetPtr, size_t *sizePtr,
                       void *ptr, bool doTransferOnCpu, uint32_t rootDeviceIndex);

    MemObjOffsetArray offset = {};
    MemObjSizeArray size = {};
    MemObj *memObj = nullptr;
    void *ptr = nullptr;
    void *lockedPtr = nullptr;
    cl_command_type cmdType = 0;
    cl_map_flags mapFlags = 0;
    uint32_t mipLevel = 0;
    uint32_t mipPtrOffset = 0;
    bool blocking = false;
    bool doTransferOnCpu = false;

    void *getCpuPtrForReadWrite();
};

struct MapInfo {
    MapInfo() = default;
    MapInfo(void *ptr, size_t ptrLength, MemObjSizeArray size, MemObjOffsetArray offset, uint32_t mipLevel)
        : size(size), offset(offset), ptrLength(ptrLength), ptr(ptr), mipLevel(mipLevel) {
    }

    MemObjSizeArray size = {};
    MemObjOffsetArray offset = {};
    size_t ptrLength = 0;
    void *ptr = nullptr;
    uint32_t mipLevel = 0;
    bool readOnly = false;
    GraphicsAllocation *graphicsAllocation = nullptr;
};
} // namespace NEO

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue_hw_base.inl"
#include "opencl/source/memory_manager/resource_surface.h"

namespace NEO {

template <>
void CommandQueueHw<Family>::submitCacheFlush(Surface **surfaces,
                                              size_t numSurfaces,
                                              LinearStream *commandStream,
                                              uint64_t postSyncAddress) {
    if constexpr (Family::isUsingL3Control) {
        StackVec<L3Range, 128> subranges;
        for (auto surface : createRange(surfaces, numSurfaces)) {
            auto resource = reinterpret_cast<ResourceSurface *>(surface);
            auto alloc = resource->getGraphicsAllocation();
            coverRangeExact(alloc->getGpuAddress(), alloc->getUnderlyingBufferSize(), subranges, resource->resourceType);
        }

        for (size_t subrangeNumber = 0; subrangeNumber < subranges.size(); subrangeNumber += maxFlushSubrangeCount) {
            size_t rangeCount = subranges.size() <= subrangeNumber + maxFlushSubrangeCount ? subranges.size() - subrangeNumber : maxFlushSubrangeCount;
            Range<L3Range> range = createRange(subranges.begin() + subrangeNumber, rangeCount);
            uint64_t postSyncAddressToFlush = 0;
            if (rangeCount < maxFlushSubrangeCount || subranges.size() - subrangeNumber - maxFlushSubrangeCount == 0) {
                postSyncAddressToFlush = postSyncAddress;
            }

            flushGpuCache<Family>(commandStream, range, postSyncAddressToFlush, device->getHardwareInfo());
        }
    }
}

template <>
bool CommandQueueHw<Family>::isCacheFlushCommand(uint32_t commandType) const {
    return commandType == CL_COMMAND_RESOURCE_BARRIER;
}

template <>
LinearStream &getCommandStream<Family, CL_COMMAND_RESOURCE_BARRIER>(CommandQueue &commandQueue, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounterCmdsSpace, bool blitEnqueue, const MultiDispatchInfo &multiDispatchInfo, Surface **surfaces, size_t numSurfaces, bool isMarkerWithProfiling, bool eventsInWaitList) {
    size_t expectedSizeCS = 0;
    [[maybe_unused]] bool usePostSync = false;
    if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSize<Family>(csrDeps);
        usePostSync = true;
    }

    if constexpr (Family::isUsingL3Control) {
        StackVec<L3Range, 128> subranges;
        for (auto surface : createRange(surfaces, numSurfaces)) {
            ResourceSurface *resource = reinterpret_cast<ResourceSurface *>(surface);
            auto alloc = resource->getGraphicsAllocation();
            coverRangeExact(alloc->getGpuAddress(), alloc->getUnderlyingBufferSize(), subranges, resource->resourceType);
        }
        expectedSizeCS += getSizeNeededToFlushGpuCache<Family>(subranges, usePostSync);
    }

    return commandQueue.getCS(expectedSizeCS);
}

} // namespace NEO

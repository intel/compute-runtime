/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flush_caches_bitmask.h"

template <>
template <typename WalkerType>
void NEO::GpgpuWalkerHelper<NEO::Family>::setupTimestampPacketFlushL3(WalkerType &walkerCmd, NEO::CommandQueue &commandQueue, const FlushL3Args &args) {

    if constexpr (std::is_same_v<WalkerType, typename NEO::Family::COMPUTE_WALKER_2>) {

        bool l2Flush = args.usingSharedObjects;
        bool l2TransientFlush = args.usingSystemAllocation || args.containsPrintBuffer;

        bool pendingL2Flush = l2Flush || l2TransientFlush;
        bool flushInPostSync = args.signalEvent || args.blocking || args.containsPrintBuffer;

        auto flushCachesMask = debugManager.flags.FlushAllCaches.get();
        if (flushCachesMask) {
            if (flushCachesMask & NEO::FlushCachesBitmask::l2Flush) {
                flushInPostSync = true;
                l2Flush = true;
            }
            if (flushCachesMask & NEO::FlushCachesBitmask::l2TransientFlush) {
                flushInPostSync = true;
                l2TransientFlush = true;
            }
        }

        if (flushInPostSync) {
            auto &postSyncData = walkerCmd.getPostSync();
            postSyncData.setL2Flush(l2Flush);
            postSyncData.setL2TransientFlush(l2TransientFlush);
        } else if (pendingL2Flush) {
            commandQueue.setPendingL3FlushForHostVisibleResources(true);
        }
    }
}

namespace NEO {

template <>
size_t EnqueueOperation<Family>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    if (isCommandWithoutKernel(cmdType)) {
        return EnqueueOperation<Family>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    } else {
        return EnqueueOperation<Family>::getSizeRequiredCSKernel<Family::COMPUTE_WALKER_2>(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel, dispatchInfo);
    }
}

} // namespace NEO

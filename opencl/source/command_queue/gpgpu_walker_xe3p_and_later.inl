/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
template <typename WalkerType>
void NEO::GpgpuWalkerHelper<NEO::Family>::setupTimestampPacketFlushL3(WalkerType &walkerCmd, NEO::CommandQueue &commandQueue, const FlushL3Args &args) {

    if constexpr (std::is_same_v<WalkerType, typename NEO::Family::COMPUTE_WALKER_2>) {

        bool l2Flush = args.usingSharedObjects;
        bool l2TransientFlush = args.usingSystemAllocation || args.containsPrintBuffer;

        bool pendingL2Flush = l2Flush || l2TransientFlush;
        bool flushInPostSync = args.signalEvent || args.blocking || args.containsPrintBuffer;

        if (debugManager.flags.RedirectFlushL3HostUsmToExternal.get()) {
            l2Flush = l2TransientFlush;
            l2TransientFlush = false;
        }

        if (debugManager.flags.FlushAllCaches.get()) {
            flushInPostSync = true;
            l2Flush = true;
            l2TransientFlush = true;
        }

        if (debugManager.flags.ForceFlushL3AfterPostSyncForExternalAllocation.get()) {
            flushInPostSync = true;
            l2Flush = true;
        }
        if (debugManager.flags.ForceFlushL3AfterPostSyncForHostUsm.get()) {
            flushInPostSync = true;
            l2TransientFlush = true;
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

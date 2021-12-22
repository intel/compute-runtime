/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush.inl"
#include "shared/source/helpers/pipe_control_args.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                    const size_t *pRangeSizes,
                                                                    const void **pRanges) {

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    const auto &hwInfo = this->device->getHwInfo();
    bool supportL3Control = hwInfo.capabilityTable.supportCacheFlushAfterWalker;
    if (!supportL3Control) {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(),
                                                                      args);
    } else {
        NEO::LinearStream *commandStream = commandContainer.getCommandStream();
        NEO::SVMAllocsManager *svmAllocsManager =
            device->getDriverHandle()->getSvmAllocsManager();

        for (uint32_t i = 0; i < numRanges; i++) {
            StackVec<NEO::L3Range, NEO::maxFlushSubrangeCount> subranges;
            const uint64_t pRange = reinterpret_cast<uint64_t>(pRanges[i]);
            const size_t pRangeSize = pRangeSizes[i];
            const uint64_t pEndRange = pRange + pRangeSize;
            uint64_t pFlushRange;
            size_t pFlushRangeSize;
            uint64_t postSyncAddressToFlush = 0;
            NEO::SvmAllocationData *allocData =
                svmAllocsManager->getSVMAllocs()->get(pRanges[i]);

            if (allocData == nullptr || pRangeSize > allocData->size) {
                continue;
            }

            pFlushRange = pRange;
            pFlushRangeSize = pRangeSize;

            if (NEO::L3Range::meetsMinimumAlignment(pRange) == false) {
                pFlushRange = alignDown(pRange, MemoryConstants::pageSize);
            }
            if (NEO::L3Range::meetsMinimumAlignment(pRangeSize) == false) {
                pFlushRangeSize = alignUp(pRangeSize, MemoryConstants::pageSize);
            }

            bool isRangeSharedBetweenTwoPages =
                (alignDown(pEndRange, MemoryConstants::pageSize) !=
                 pFlushRange);
            if (isRangeSharedBetweenTwoPages) {
                pFlushRangeSize += MemoryConstants::pageSize;
            }

            coverRangeExact(pFlushRange,
                            pFlushRangeSize,
                            subranges,
                            GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);

            NEO::flushGpuCache<GfxFamily>(commandStream, subranges,
                                          postSyncAddressToFlush,
                                          hwInfo);
        }
    }
}
} // namespace L0

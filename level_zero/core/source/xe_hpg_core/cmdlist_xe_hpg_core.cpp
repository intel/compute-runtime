/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_dg2_and_pvc.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template <>
void CommandListCoreFamily<IGFX_XE_HPG_CORE>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                       const size_t *pRangeSizes,
                                                                       const void **pRanges) {

    NEO::LinearStream *commandStream = commandContainer.getCommandStream();
    NEO::SVMAllocsManager *svmAllocsManager =
        device->getDriverHandle()->getSvmAllocsManager();

    StackVec<NEO::L3Range, NEO::maxFlushSubrangeCount> subranges;
    uint64_t postSyncAddressToFlush = 0;
    for (uint32_t i = 0; i < numRanges; i++) {
        uint64_t pRange = castToUint64(pRanges[i]);
        size_t pRangeSize = pRangeSizes[i];
        uint64_t pFlushRange;
        size_t pFlushRangeSize;
        NEO::SvmAllocationData *allocData =
            svmAllocsManager->getSVMAllocs()->get(pRanges[i]);

        if (allocData == nullptr || pRangeSize > allocData->size) {
            continue;
        }

        pFlushRange = pRange;

        if (NEO::L3Range::meetsMinimumAlignment(pRange) == false) {
            pFlushRange = alignDown(pRange, MemoryConstants::pageSize);
        }
        pRangeSize = (pRange + pRangeSize) - pFlushRange;
        pFlushRangeSize = pRangeSize;
        if (NEO::L3Range::meetsMinimumAlignment(pRangeSize) == false) {
            pFlushRangeSize = alignUp(pRangeSize, MemoryConstants::pageSize);
        }
        coverRangeExact(pFlushRange,
                        pFlushRangeSize,
                        subranges,
                        GfxFamily::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION);
    }
    for (size_t subrangeNumber = 0; subrangeNumber < subranges.size(); subrangeNumber += NEO::maxFlushSubrangeCount) {
        size_t rangeCount = subranges.size() <= subrangeNumber + NEO::maxFlushSubrangeCount ? subranges.size() - subrangeNumber : NEO::maxFlushSubrangeCount;
        NEO::Range<NEO::L3Range> range = createRange(subranges.begin() + subrangeNumber, rangeCount);

        NEO::flushGpuCache<GfxFamily>(commandStream, range, postSyncAddressToFlush, device->getHwInfo());
    }
}

template struct CommandListCoreFamily<IGFX_XE_HPG_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPG_CORE>;

} // namespace L0

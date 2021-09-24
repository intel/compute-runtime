/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/gen12lp/helpers_gen12lp.h"

namespace NEO {
namespace Gen12LPHelpers {

void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream) {}

uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation && graphicsAllocation->getMemoryPool() == MemoryPool::LocalMemory) {
        return BIT(PageTableEntry::localMemoryBit);
    }
    return 0;
}

void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data) {
    data.localMemory = commandStreamReceiver.isLocalMemoryEnabled();
}

bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

} // namespace Gen12LPHelpers
} // namespace NEO

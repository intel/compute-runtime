/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/tbx/tbx_proto.h"

#include "third_party/aub_stream/headers/aubstream.h"

namespace NEO {

uint64_t AubHelper::getTotalMemBankSize() {
    return 32ull * MemoryConstants::gigaByte;
}

int AubHelper::getMemTrace(uint64_t pdEntryBits) {
    if (pdEntryBits & BIT(PageTableEntry::localMemoryBit)) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    pdEntryBits &= ~BIT(PageTableEntry::localMemoryBit);
    return pdEntryBits;
}

uint32_t AubHelper::getMemType(uint32_t addressSpace) {
    if (addressSpace == AubMemDump::AddressSpaceValues::TraceLocal) {
        return mem_types::MEM_TYPE_LOCALMEM;
    }
    return mem_types::MEM_TYPE_SYSTEM;
}

uint64_t AubHelper::getPerTileLocalMemorySize(const HardwareInfo *pHwInfo) {
    if (DebugManager.flags.HBMSizePerTileInGigabytes.get() > 0) {
        return DebugManager.flags.HBMSizePerTileInGigabytes.get() * MemoryConstants::gigaByte;
    }
    return getTotalMemBankSize() / HwHelper::getSubDevicesCount(pHwInfo);
}
} // namespace NEO

/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw_base.inl"
#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {

template <typename GfxFamily>
constexpr uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getMaskAndValueForPollForCompletion() {
    return 0x00008000;
}

template <typename GfxFamily>
uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getGUCWorkQueueItemHeader() {
    if (EngineHelpers::isCcs(osContext->getEngineType())) {
        return 0x00030401;
    }
    return 0x00030001;
}

template <typename GfxFamily>
int AUBCommandStreamReceiverHw<GfxFamily>::getAddressSpaceFromPTEBits(uint64_t entryBits) const {
    if (entryBits & BIT(PageTableEntry::localMemoryBit)) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

} // namespace NEO

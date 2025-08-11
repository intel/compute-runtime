/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw_base.inl"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/memory_manager/memory_pool.h"

namespace NEO {
typedef Gen12LpFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
int AUBCommandStreamReceiverHw<Family>::getAddressSpaceFromPTEBits(uint64_t entryBits) const {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}
template <>
constexpr uint32_t AUBCommandStreamReceiverHw<Family>::getMaskAndValueForPollForCompletion() {
    return 0x00008000;
}

template <>
void populateFactoryTable<AUBCommandStreamReceiverHw<Family>>() {
    extern AubCommandStreamReceiverCreateFunc aubCommandStreamReceiverFactory[IGFX_MAX_CORE];
    UNRECOVERABLE_IF(!isInRange(gfxCore, aubCommandStreamReceiverFactory));
    aubCommandStreamReceiverFactory[gfxCore] = AUBCommandStreamReceiverHw<Family>::create;
}

template <>
uint32_t AUBCommandStreamReceiverHw<Family>::getGUCWorkQueueItemHeader() {
    if (aub_stream::ENGINE_CCS == osContext->getEngineType()) {
        return 0x00030401;
    }
    return 0x00030001;
}

template class AUBCommandStreamReceiverHw<Family>;
static_assert(NEO::NonCopyableAndNonMovable<NEO::AUBCommandStreamReceiverHw<Family>>);
} // namespace NEO

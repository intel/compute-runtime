/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw_base.inl"

namespace NEO {

template <typename GfxFamily>
constexpr uint32_t AUBCommandStreamReceiverHw<GfxFamily>::getMaskAndValueForPollForCompletion() {
    return 0x100;
}

template <typename GfxFamily>
int AUBCommandStreamReceiverHw<GfxFamily>::getAddressSpaceFromPTEBits(uint64_t entryBits) const {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw_xehp_and_later.inl"

namespace NEO {
using Family = XE_HPC_COREFamily;

template <>
bool CommandStreamReceiverSimulatedCommonHw<Family>::expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length) {
    auto format = static_cast<uint32_t>(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
    UNRECOVERABLE_IF(format > 0x1F);

    uint32_t value = (format << 3); // [3:7] compression_format
    value |= 0;                     // [0] disable
    this->writeMMIO(0x519C, value);
    this->writeMMIO(0xB0F0, value);
    this->writeMMIO(0xE4C0, value);

    bool ret = this->expectMemory(gfxAddress, srcAddress, length,
                                  AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);

    value |= 1; // [0] enable
    this->writeMMIO(0x519C, value);
    this->writeMMIO(0xB0F0, value);
    this->writeMMIO(0xE4C0, value);

    return ret;
}

template class CommandStreamReceiverSimulatedCommonHw<Family>;
} // namespace NEO

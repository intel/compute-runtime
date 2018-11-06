/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/physical_address_allocator.h"

namespace AubMemDump {
struct AubStream;
}

namespace OCLRT {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedCommonHw : public CommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;

  public:
    uint64_t getGTTBits() const {
        return 0u;
    }
    void initGlobalMMIO();
    void initAdditionalMMIO();

    AubMemDump::AubStream *stream;

  protected:
    MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
    PhysicalAddressAllocator *createPhysicalAddressAllocator();
};
} // namespace OCLRT

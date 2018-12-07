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
  protected:
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<GfxFamily>::osContext;

  public:
    uint64_t getGTTBits() const {
        return 0u;
    }
    void initGlobalMMIO();
    void initAdditionalMMIO();
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;
    size_t getEngineIndexFromInstance(EngineInstanceT engineInstance);
    size_t getEngineIndex(EngineType engineType);

    AubMemDump::AubStream *stream;
    size_t gpgpuEngineIndex = EngineInstanceConstants::numGpgpuEngineInstances - 1;

  protected:
    MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
    PhysicalAddressAllocator *createPhysicalAddressAllocator();
};
} // namespace OCLRT

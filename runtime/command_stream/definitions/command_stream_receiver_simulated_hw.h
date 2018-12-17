/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/physical_address_allocator.h"

namespace OCLRT {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedHw : public CommandStreamReceiverSimulatedCommonHw<GfxFamily> {
  protected:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::CommandStreamReceiverSimulatedCommonHw;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::osContext;

  public:
    uint32_t getMemoryBank(GraphicsAllocation *allocation) const {
        return MemoryBanks::getBank(this->deviceIndex);
    }
    int getAddressSpace(int hint) {
        return AubMemDump::AddressSpaceValues::TraceNonlocal;
    }
    PhysicalAddressAllocator *createPhysicalAddressAllocator(const HardwareInfo *hwInfo) {
        return new PhysicalAddressAllocator();
    }
};
} // namespace OCLRT

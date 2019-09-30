/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/physical_address_allocator.h"

namespace NEO {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedHw : public CommandStreamReceiverSimulatedCommonHw<GfxFamily> {
  protected:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::CommandStreamReceiverSimulatedCommonHw;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::osContext;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getDeviceIndex;

  public:
    uint32_t getMemoryBank(GraphicsAllocation *allocation) const {
        return MemoryBanks::getBank(getDeviceIndex());
    }
    int getAddressSpace(int hint) {
        return AubMemDump::AddressSpaceValues::TraceNonlocal;
    }
    PhysicalAddressAllocator *createPhysicalAddressAllocator(const HardwareInfo *hwInfo) {
        return new PhysicalAddressAllocator();
    }
    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation) override{};

    void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation) override {
        graphicsAllocation.setAubWritable(writable, getMemoryBank(&graphicsAllocation));
    }
    bool isAubWritable(GraphicsAllocation &graphicsAllocation) const override {
        return graphicsAllocation.isAubWritable(getMemoryBank(&graphicsAllocation));
    }
    void setTbxWritable(bool writable, GraphicsAllocation &graphicsAllocation) override {
        graphicsAllocation.setTbxWritable(writable, getMemoryBank(&graphicsAllocation));
    }
    bool isTbxWritable(GraphicsAllocation &graphicsAllocation) const override {
        return graphicsAllocation.isTbxWritable(getMemoryBank(&graphicsAllocation));
    }
};
} // namespace NEO

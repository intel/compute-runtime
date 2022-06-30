/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"

namespace NEO {

template <typename GfxFamily>
class MockSimulatedCsrHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  public:
    using CommandStreamReceiverSimulatedHw<GfxFamily>::CommandStreamReceiverSimulatedHw;
    using CommandStreamReceiverSimulatedHw<GfxFamily>::localMemoryEnabled;
    using CommandStreamReceiverSimulatedHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemory;
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {
    }
    void pollForCompletion() override {
    }
    void initializeEngine() override {
    }
    bool writeMemory(GraphicsAllocation &gfxAllocation) override {
        return true;
    }
    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation) override {
        CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemoryWithAubManager(graphicsAllocation);
    }
    void writeMMIO(uint32_t offset, uint32_t value) override {}
    bool isMultiOsContextCapable() const override {
        return multiOsContextCapable;
    }
    void dumpAllocation(GraphicsAllocation &graphicsAllocation) override {}
    bool multiOsContextCapable = false;
};

} // namespace NEO

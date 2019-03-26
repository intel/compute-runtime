/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
#include <stdint.h>

namespace NEO {

class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
class MemoryManager;

class ExperimentalCommandBuffer {
  public:
    virtual ~ExperimentalCommandBuffer();
    ExperimentalCommandBuffer(CommandStreamReceiver *csr, double profilingTimerResolution);

    template <typename GfxFamily>
    void injectBufferStart(LinearStream &parentStream, size_t cmdBufferOffset);

    template <typename GfxFamily>
    size_t getRequiredInjectionSize() noexcept;

    template <typename GfxFamily>
    size_t programExperimentalCommandBuffer();

    void makeResidentAllocations();

  protected:
    template <typename GfxFamily>
    size_t getTotalExperimentalSize() noexcept;

    void getCS(size_t minRequiredSize);

    template <typename GfxFamily>
    void addTimeStampPipeControl();

    template <typename GfxFamily>
    size_t getTimeStampPipeControlSize() noexcept;

    template <typename GfxFamily>
    void addExperimentalCommands();

    template <typename GfxFamily>
    size_t getExperimentalCommandsSize() noexcept;

    CommandStreamReceiver *commandStreamReceiver;
    std::unique_ptr<LinearStream> currentStream;

    GraphicsAllocation *timestamps;
    uint32_t timestampsOffset;

    GraphicsAllocation *experimentalAllocation;
    uint32_t experimentalAllocationOffset;

    bool defaultPrint;
    double timerResolution;
};

} // namespace NEO

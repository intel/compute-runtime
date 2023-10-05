/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
#include <memory>

namespace NEO {

class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
class MemoryManager;

class ExperimentalCommandBuffer : NonCopyableOrMovableClass {
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
    uint32_t timestampsOffset = 0;

    GraphicsAllocation *experimentalAllocation;
    uint32_t experimentalAllocationOffset = 0;

    bool defaultPrint = true;
    double timerResolution;
};

} // namespace NEO

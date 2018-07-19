/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <memory>
#include <stdint.h>

namespace OCLRT {

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

} // namespace OCLRT

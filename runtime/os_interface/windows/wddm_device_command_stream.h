/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
#include "runtime/command_stream/device_command_stream.h"

struct COMMAND_BUFFER_HEADER_REC;

namespace OCLRT {
class GmmPageTableMngr;
class GraphicsAllocation;
class WddmMemoryManager;
class Wddm;

template <typename GfxFamily>
class WddmCommandStreamReceiver : public DeviceCommandStreamReceiver<GfxFamily> {
    typedef DeviceCommandStreamReceiver<GfxFamily> BaseClass;
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiver::memoryManager;

  public:
    WddmCommandStreamReceiver(const HardwareInfo &hwInfoIn, Wddm *wddm, ExecutionEnvironment &executionEnvironment);
    virtual ~WddmCommandStreamReceiver();

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void makeResident(GraphicsAllocation &gfxAllocation) override;
    void processResidency(ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void processEviction() override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait, OsContext &osContext) override;

    WddmMemoryManager *getMemoryManager();
    MemoryManager *createMemoryManager(bool enable64kbPages);
    Wddm *peekWddm() {
        return wddm;
    }
    GmmPageTableMngr *createPageTableManager() override;

  protected:
    void initPageTableManagerRegisters(LinearStream &csr) override;
    void kmDafLockAllocations(ResidencyContainer *allocationsForResidency);

    Wddm *wddm;
    COMMAND_BUFFER_HEADER_REC *commandBufferHeader;
    bool pageTableManagerInitialized = false;
};
} // namespace OCLRT

/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
    WddmCommandStreamReceiver(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment);
    virtual ~WddmCommandStreamReceiver();

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency, OsContext &osContext) override;
    void makeResident(GraphicsAllocation &gfxAllocation) override;
    void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) override;
    void processEviction() override;
    bool waitForFlushStamp(FlushStamp &flushStampToWait, OsContext &osContext) override;

    WddmMemoryManager *getMemoryManager();
    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory);
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

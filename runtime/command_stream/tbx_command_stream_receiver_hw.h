/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper.h"
#include "command_stream_receiver_simulated_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/page_table.h"

namespace OCLRT {

class TbxStream;

class TbxMemoryManager : public OsAgnosticMemoryManager {
  public:
    TbxMemoryManager(bool enable64kbPages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment) {}
    uint64_t getSystemSharedMemory() override {
        return 1 * GB;
    }
};

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using BaseClass::engineIndex;
    using BaseClass::osContext;

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContext;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfoTable;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeCoherent(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer &allocationsForResidency) override;
    void waitBeforeMakingNonResidentWhenRequired() override;
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits, DevicesBitfield devicesBitfield);
    bool writeMemory(GraphicsAllocation &gfxAllocation);

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits);
    MOCKABLE_VIRTUAL void pollForCompletion();

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment);

    TbxCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment);
    ~TbxCommandStreamReceiverHw() override;

    void initializeEngine();

    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override {
        return new TbxMemoryManager(enable64kbPages, enableLocalMemory, this->executionEnvironment);
    }
    TbxMemoryManager *getMemoryManager() {
        return (TbxMemoryManager *)CommandStreamReceiver::getMemoryManager();
    }

    TbxStream tbxStream;

    uint32_t aubDeviceId;
    bool streamInitialized = false;

    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper gttRemap;

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_TBX;
    }
};
} // namespace OCLRT

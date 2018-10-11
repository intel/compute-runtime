/*
 * Copyright (C) 2017-2018 Intel Corporation
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

class TbxMemoryManager : public OsAgnosticMemoryManager {
  public:
    TbxMemoryManager(bool enable64kbPages, bool enableLocalMemory, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment) {}
    uint64_t getSystemSharedMemory() override {
        return 1 * GB;
    }
};

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    typedef typename OCLRT::AUBFamilyMapper<GfxFamily>::AUB AUB;
    typedef typename AUB::MiContextDescriptorReg MiContextDescriptorReg;

  public:
    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override;
    void makeCoherent(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) override;
    void waitBeforeMakingNonResidentWhenRequired() override;
    bool writeMemory(GraphicsAllocation &gfxAllocation);

    // Family specific version
    void submitLRCA(EngineType engineType, const MiContextDescriptorReg &contextDescriptor);
    void pollForCompletion(EngineType engineType);
    void initGlobalMMIO();
    void initEngineMMIO(EngineType engineType);

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment);

    TbxCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment);
    ~TbxCommandStreamReceiverHw() override;

    void initializeEngine(EngineType engineType);

    static const AubMemDump::LrcaHelper &getCsTraits(EngineType engineType);

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRCS;
        uint32_t ggttRCS;
        size_t sizeRCS;
        uint32_t tailRCS;
    } engineInfoTable[EngineType::NUM_ENGINES];

    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override {
        return new TbxMemoryManager(enable64kbPages, enableLocalMemory, this->executionEnvironment);
    }
    TbxMemoryManager *getMemoryManager() {
        return (TbxMemoryManager *)CommandStreamReceiver::getMemoryManager();
    }
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;

    TbxCommandStreamReceiver::TbxStream stream;
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

/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper.h"
#include "command_stream_receiver_simulated_hw.h"
#include "runtime/aub/aub_center.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/helpers/array_count.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/page_table.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"

namespace OCLRT {

class AubSubCaptureManager;

template <typename GfxFamily>
class AUBCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    typedef typename AUBFamilyMapper<GfxFamily>::AUB AUB;
    typedef typename AUB::MiContextDescriptorReg MiContextDescriptorReg;
    using ExternalAllocationsContainer = std::vector<AllocationView>;

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) override;

    void makeResidentExternal(AllocationView &allocationView);
    void makeNonResidentExternal(uint64_t gpuAddress);

    AubMemDump::AubFileStream *getAubStream() const {
        return static_cast<AubMemDump::AubFileStream *>(this->stream);
    }

    MOCKABLE_VIRTUAL bool writeMemory(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL bool writeMemory(AllocationView &allocationView);
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue);

    void expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length);
    void expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length);

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    // Family specific version
    void submitLRCA(EngineInstanceT engineInstance, const MiContextDescriptorReg &contextDescriptor);
    MOCKABLE_VIRTUAL void pollForCompletion(EngineInstanceT engineInstance);
    void initEngineMMIO(EngineInstanceT engineInstance);

    uint32_t getDumpHandle();
    MOCKABLE_VIRTUAL void addContextToken(uint32_t dumpHandle);

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment);

    AUBCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment);
    ~AUBCommandStreamReceiverHw() override;

    AUBCommandStreamReceiverHw(const AUBCommandStreamReceiverHw &) = delete;
    AUBCommandStreamReceiverHw &operator=(const AUBCommandStreamReceiverHw &) = delete;

    MOCKABLE_VIRTUAL void openFile(const std::string &fileName);
    MOCKABLE_VIRTUAL bool reopenFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void initFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void closeFile();
    MOCKABLE_VIRTUAL bool isFileOpen() const;
    MOCKABLE_VIRTUAL const std::string &getFileName();

    void initializeEngine(size_t engineIndex);
    void freeEngineInfoTable();

    MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) override {
        return new OsAgnosticMemoryManager(enable64kbPages, enableLocalMemory, true, this->executionEnvironment);
    }

    static const AubMemDump::LrcaHelper &getCsTraits(EngineInstanceT engineInstance);
    size_t getEngineIndexFromInstance(EngineInstanceT engineInstance);
    size_t getEngineIndex(EngineType engineType);

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRingBuffer;
        uint32_t ggttRingBuffer;
        size_t sizeRingBuffer;
        uint32_t tailRingBuffer;
    } engineInfoTable[arrayCount(allEngineInstances)] = {};
    size_t gpgpuEngineIndex = arrayCount(gpgpuEngineInstances) - 1;

    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    uint32_t aubDeviceId;
    bool standalone;

    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper *gttRemap;

    void setCsrProgrammingMode(void){};
    MOCKABLE_VIRTUAL bool addPatchInfoComments();
    void addGUCStartMessage(uint64_t batchBufferAddress, EngineType engineType);
    uint32_t getGUCWorkQueueItemHeader(EngineType engineType);
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_AUB;
    }

    int getAddressSpaceFromPTEBits(uint64_t entryBits) const;

    size_t getPreferredTagPoolSize() const override { return 1; }

  protected:
    MOCKABLE_VIRTUAL void expectMemory(void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);
    bool dumpAubNonWritable = false;
    ExternalAllocationsContainer externalAllocations;
};
} // namespace OCLRT

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub/aub_center.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/helpers/array_count.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/page_table.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "runtime/utilities/spinlock.h"

#include "command_stream_receiver_simulated_hw.h"

namespace OCLRT {

class AubSubCaptureManager;

template <typename GfxFamily>
class AUBCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using ExternalAllocationsContainer = std::vector<AllocationView>;
    using BaseClass::getParametersForWriteMemory;
    using BaseClass::osContext;

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer &allocationsForResidency) override;

    void makeResidentExternal(AllocationView &allocationView);
    void makeNonResidentExternal(uint64_t gpuAddress);

    AubMemDump::AubFileStream *getAubStream() const {
        return static_cast<AubMemDump::AubFileStream *>(this->stream);
    }

    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation) override;
    MOCKABLE_VIRTUAL bool writeMemory(AllocationView &allocationView);
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue);
    void expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override;

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits);
    void pollForCompletion() override;
    void pollForCompletionImpl() override;
    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) override;

    uint32_t getDumpHandle();
    MOCKABLE_VIRTUAL void addContextToken(uint32_t dumpHandle);
    void dumpAllocation(GraphicsAllocation &gfxAllocation);

    static CommandStreamReceiver *create(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment);

    AUBCommandStreamReceiverHw(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment);
    ~AUBCommandStreamReceiverHw() override;

    AUBCommandStreamReceiverHw(const AUBCommandStreamReceiverHw &) = delete;
    AUBCommandStreamReceiverHw &operator=(const AUBCommandStreamReceiverHw &) = delete;

    MOCKABLE_VIRTUAL void openFile(const std::string &fileName);
    MOCKABLE_VIRTUAL bool reopenFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void initFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void closeFile();
    MOCKABLE_VIRTUAL bool isFileOpen() const;
    MOCKABLE_VIRTUAL const std::string getFileName();

    MOCKABLE_VIRTUAL void initializeEngine();
    AubSubCaptureManager *subCaptureManager = nullptr;
    uint32_t aubDeviceId;
    bool standalone;

    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper *gttRemap;

    MOCKABLE_VIRTUAL bool addPatchInfoComments();
    void addGUCStartMessage(uint64_t batchBufferAddress);
    uint32_t getGUCWorkQueueItemHeader();

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_AUB;
    }

    int getAddressSpaceFromPTEBits(uint64_t entryBits) const;

    size_t getPreferredTagPoolSize() const override { return 1; }

  protected:
    constexpr static uint32_t getMaskAndValueForPollForCompletion();

    bool dumpAubNonWritable = false;
    ExternalAllocationsContainer externalAllocations;

    uint32_t pollForCompletionTaskCount = 0u;
    SpinLock pollForCompletionLock;
};
} // namespace OCLRT

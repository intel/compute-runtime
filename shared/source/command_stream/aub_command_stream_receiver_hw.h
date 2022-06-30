/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/memory_manager/page_table.h"
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/source/utilities/spinlock.h"

#include "aub_mapper.h"

namespace NEO {

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
    using BaseClass::peekExecutionEnvironment;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override;

    void makeResidentExternal(AllocationView &allocationView);
    void makeNonResidentExternal(uint64_t gpuAddress);

    AubMemDump::AubFileStream *getAubStream() const {
        return static_cast<AubMemDump::AubFileStream *>(this->stream);
    }

    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation) override;
    MOCKABLE_VIRTUAL bool writeMemory(AllocationView &allocationView);
    void writeMMIO(uint32_t offset, uint32_t value) override;
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue);
    bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;
    void addAubComment(const char *message) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBufferAub(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits);
    void pollForCompletion() override;
    void pollForCompletionImpl() override;
    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override;

    uint32_t getDumpHandle();
    MOCKABLE_VIRTUAL void addContextToken(uint32_t dumpHandle);
    void dumpAllocation(GraphicsAllocation &gfxAllocation) override;

    static CommandStreamReceiver *create(const std::string &fileName,
                                         bool standalone,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield);

    AUBCommandStreamReceiverHw(const std::string &fileName,
                               bool standalone,
                               ExecutionEnvironment &executionEnvironment,
                               uint32_t rootDeviceIndex,
                               const DeviceBitfield deviceBitfield);
    ~AUBCommandStreamReceiverHw() override;

    AUBCommandStreamReceiverHw(const AUBCommandStreamReceiverHw &) = delete;
    AUBCommandStreamReceiverHw &operator=(const AUBCommandStreamReceiverHw &) = delete;

    MOCKABLE_VIRTUAL void openFile(const std::string &fileName);
    MOCKABLE_VIRTUAL bool reopenFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void initFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void closeFile();
    MOCKABLE_VIRTUAL bool isFileOpen() const;
    MOCKABLE_VIRTUAL const std::string getFileName();

    void initializeEngine() override;
    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
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

  protected:
    constexpr static uint32_t getMaskAndValueForPollForCompletion();

    bool dumpAubNonWritable = false;
    bool isEngineInitialized = false;
    ExternalAllocationsContainer externalAllocations;

    uint32_t pollForCompletionTaskCount = 0u;
    SpinLock pollForCompletionLock;
};
} // namespace NEO

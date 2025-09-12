/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/memory_manager/residency_container.h"

#include <mutex>

namespace NEO {
class PDPE;
class PML4;

class AubSubCaptureManager;

template <typename GfxFamily>
class AUBCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using ExternalAllocationsContainer = std::vector<AllocationView>;
    using BaseClass::getParametersForMemory;
    using BaseClass::osContext;
    using BaseClass::pollForCompletion;

  public:
    using BaseClass::peekExecutionEnvironment;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::writeMemory;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) override;

    void makeResidentExternal(AllocationView &allocationView);
    void makeNonResidentExternal(uint64_t gpuAddress);

    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override;
    MOCKABLE_VIRTUAL bool writeMemory(AllocationView &allocationView);
    void writeMMIO(uint32_t offset, uint32_t value) override;
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue);
    bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;
    void addAubComment(const char *message) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBufferAub(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits);
    void pollForCompletion(bool skipTaskCountCheck) override;
    void pollForCompletionImpl() override;
    void pollForAubCompletion() override { pollForCompletion(true); };
    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override;

    uint32_t getDumpHandle();
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

    MOCKABLE_VIRTUAL void openFile(const std::string &fileName);
    MOCKABLE_VIRTUAL bool reopenFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void initFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void closeFile();
    MOCKABLE_VIRTUAL bool isFileOpen() const;
    MOCKABLE_VIRTUAL const std::string getFileName();

    void initializeEngine() override;
    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    bool standalone;

    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;
    uint32_t getGUCWorkQueueItemHeader();

    CommandStreamReceiverType getType() const override {
        return CommandStreamReceiverType::aub;
    }

    int getAddressSpaceFromPTEBits(uint64_t entryBits) const;

    std::mutex mutex;

    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<std::mutex> lockStream() {
        return std::unique_lock<std::mutex>(mutex);
    }

  protected:
    constexpr static uint32_t getMaskAndValueForPollForCompletion();

    bool dumpAubNonWritable = false;
    bool isEngineInitialized = false;
    ExternalAllocationsContainer externalAllocations;

    TaskCountType pollForCompletionTaskCount = 0u;
    SpinLock pollForCompletionLock;
};
} // namespace NEO

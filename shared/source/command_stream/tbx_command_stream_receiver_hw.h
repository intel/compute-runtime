/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/page_table.h"

#include <array>
#include <set>

namespace NEO {

class AubSubCaptureManager;
class TbxStream;
class CpuPageFaultManager;

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using BaseClass::forceSkipResourceCleanupRequired;
    using BaseClass::getParametersForMemory;
    using BaseClass::osContext;
    using BaseClass::pollForCompletion;

    uint32_t getMaskAndValueForPollForCompletion() const;
    bool getpollNotEqualValueForPollForCompletion() const;
    void flushSubmissionsAndDownloadAllocations(TaskCountType taskCount, bool skipAllocationsDownload);
    MOCKABLE_VIRTUAL uint64_t getNonBlockingDownloadTimeoutMs() const {
        return 2000; // 2s
    }

    bool isAllocTbxFaultable(GraphicsAllocation *gfxAlloc);
    void registerAllocationWithTbxFaultMngrIfTbxFaultable(GraphicsAllocation *gfxAllocation, void *cpuAddress, size_t size);
    void allowCPUMemoryAccessIfTbxFaultable(GraphicsAllocation *gfxAllocation, void *cpuAddress, size_t size);
    void protectCPUMemoryAccessIfTbxFaultable(GraphicsAllocation *gfxAllocation, void *cpuAddress, size_t size);
    void protectCPUMemoryFromWritesIfTbxFaultable(GraphicsAllocation *gfxAllocation, void *cpuAddress, size_t size);

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::peekExecutionEnvironment;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::writeMemory;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override;
    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override;
    void downloadAllocations(bool blockingWait, TaskCountType taskCount) override;
    void downloadAllocationTbx(GraphicsAllocation &gfxAllocation);
    void removeDownloadAllocation(GraphicsAllocation *alloc) override;

    void processEviction() override;
    SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override;
    void writeMMIO(uint32_t offset, uint32_t value) override;
    bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBufferTbx(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead);
    void pollForCompletion(bool skipTaskCountCheck) override;

    void dumpAllocation(GraphicsAllocation &gfxAllocation) override;

    static CommandStreamReceiver *create(const std::string &baseName,
                                         bool withAubDump,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield);

    TbxCommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                               uint32_t rootDeviceIndex,
                               const DeviceBitfield deviceBitfield);
    ~TbxCommandStreamReceiverHw() override;

    void initializeEngine() override;

    MOCKABLE_VIRTUAL MemoryManager *getMemoryManager() {
        return CommandStreamReceiver::getMemoryManager();
    }

    MOCKABLE_VIRTUAL CpuPageFaultManager *getTbxPageFaultManager();

    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    uint32_t aubDeviceId;
    bool streamInitialized = false;

    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;

    std::set<GraphicsAllocation *> allocationsForDownload = {};

    CommandStreamReceiverType getType() const override {
        return CommandStreamReceiverType::tbx;
    }

    bool dumpTbxNonWritable = false;
    bool isEngineInitialized = false;
};
} // namespace NEO

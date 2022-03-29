/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/address_mapper.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/memory_manager/page_table.h"

#include "aub_mapper.h"

#include <set>

namespace NEO {

class AubSubCaptureManager;
class TbxStream;

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using BaseClass::getParametersForWriteMemory;
    using BaseClass::osContext;

    uint32_t getMaskAndValueForPollForCompletion() const;
    bool getpollNotEqualValueForPollForCompletion() const;
    void flushSubmissionsAndDownloadAllocations(uint32_t taskCount);

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override;
    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override;
    void downloadAllocations() override;
    void downloadAllocationTbx(GraphicsAllocation &gfxAllocation);

    void processEviction() override;
    void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override;
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation) override;
    void writeMMIO(uint32_t offset, uint32_t value) override;
    bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBufferTbx(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead);
    void pollForCompletion() override;

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

    MemoryManager *getMemoryManager() {
        return CommandStreamReceiver::getMemoryManager();
    }

    TbxStream tbxStream;
    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    uint32_t aubDeviceId;
    bool streamInitialized = false;

    std::unique_ptr<PhysicalAddressAllocator> physicalAddressAllocator;
    std::unique_ptr<std::conditional<is64bit, PML4, PDPE>::type> ppgtt;
    std::unique_ptr<PDPE> ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper gttRemap;

    std::set<GraphicsAllocation *> allocationsForDownload = {};

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_TBX;
    }

    bool dumpTbxNonWritable = false;
    bool isEngineInitialized = false;
};
} // namespace NEO

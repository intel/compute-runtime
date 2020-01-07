/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/page_table.h"

#include "command_stream_receiver_simulated_hw.h"

#include <set>

namespace NEO {

class AubSubCaptureManager;
class TbxStream;

class TbxMemoryManager : public OsAgnosticMemoryManager {
  public:
    TbxMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment) {}
    uint64_t getSystemSharedMemory(uint32_t rootDeviceIndex) override {
        return 1 * GB;
    }
};

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverSimulatedHw<GfxFamily> {
  protected:
    typedef CommandStreamReceiverSimulatedHw<GfxFamily> BaseClass;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using BaseClass::getParametersForWriteMemory;
    using BaseClass::osContext;

    uint32_t getMaskAndValueForPollForCompletion() const;
    bool getpollNotEqualValueForPollForCompletion() const;

  public:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::initAdditionalMMIO;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::engineInfo;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::stream;

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) override;
    void downloadAllocation(GraphicsAllocation &gfxAllocation) override;

    void processEviction() override;
    void processResidency(const ResidencyContainer &allocationsForResidency) override;
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation) override;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    // Family specific version
    MOCKABLE_VIRTUAL void submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead);
    void pollForCompletion() override;

    static CommandStreamReceiver *create(const std::string &baseName, bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);

    TbxCommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
    ~TbxCommandStreamReceiverHw() override;

    void initializeEngine();

    TbxMemoryManager *getMemoryManager() {
        return (TbxMemoryManager *)CommandStreamReceiver::getMemoryManager();
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
};
} // namespace NEO

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/csr_definitions.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/submissions_aggregator.h"
#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/helpers/address_patch.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/completion_stamp.h"
#include "runtime/helpers/flat_batch_buffer_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/grf_config.h"
#include <cstddef>
#include <cstdint>

namespace OCLRT {
class AllocationsList;
class Device;
class EventBuilder;
class ExecutionEnvironment;
class ExperimentalCommandBuffer;
class GmmPageTableMngr;
class GraphicsAllocation;
class HostPtrSurface;
class IndirectHeap;
class InternalAllocationStorage;
class LinearStream;
class MemoryManager;
class OsContext;
class OSInterface;

enum class DispatchMode {
    DeviceDefault = 0,          //default for given device
    ImmediateDispatch,          //everything is submitted to the HW immediately
    AdaptiveDispatch,           //dispatching is handled to async thread, which combines batch buffers basing on load (not implemented)
    BatchedDispatchWithCounter, //dispatching is batched, after n commands there is implicit flush (not implemented)
    BatchedDispatch             // dispatching is batched, explicit clFlush is required
};

class CommandStreamReceiver {
  public:
    enum class SamplerCacheFlushState {
        samplerCacheFlushNotRequired,
        samplerCacheFlushBefore, //add sampler cache flush before Walker with redescribed image
        samplerCacheFlushAfter   //add sampler cache flush after Walker with redescribed image
    };
    using MutexType = std::recursive_mutex;
    CommandStreamReceiver(ExecutionEnvironment &executionEnvironment);
    virtual ~CommandStreamReceiver();

    virtual FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual void flushBatchedSubmissions() = 0;

    virtual void makeCoherent(GraphicsAllocation &gfxAllocation){};
    virtual void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency, OsContext &osContext);
    virtual void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) {}
    virtual void processEviction(OsContext &osContext);
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);
    virtual void waitBeforeMakingNonResidentWhenRequired() {}

    virtual void addPipeControl(LinearStream &commandStream, bool dcFlush) = 0;

    MemoryManager *getMemoryManager() const;
    virtual MemoryManager *createMemoryManager(bool enable64kbPages, bool enableLocalMemory) { return nullptr; }

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }

    MOCKABLE_VIRTUAL void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() { return osInterface; };

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    GraphicsAllocation *getTagAllocation() const {
        return tagAllocation;
    }
    volatile uint32_t *getTagAddress() const { return tagAddress; }

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait, OsContext &osContext) { return true; };

    uint32_t peekTaskCount() const { return taskCount; }

    uint32_t peekTaskLevel() const { return taskLevel; }

    uint32_t peekLatestSentTaskCount() const { return latestSentTaskCount; }

    uint32_t peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void enableNTo1SubmissionModel() { this->nTo1SubmissionModelEnabled = true; }
    bool isNTo1SubmissionModelEnabled() const { return this->nTo1SubmissionModelEnabled; }
    void overrideDispatchPolicy(DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    virtual void overrideMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }

    void setRequiredScratchSize(uint32_t newRequiredScratchSize);
    GraphicsAllocation *getScratchAllocation() { return scratchAllocation; }
    GraphicsAllocation *getDebugSurfaceAllocation() { return debugSurface; }
    GraphicsAllocation *allocateDebugSurface(size_t size);

    void setPreemptionCsrAllocation(GraphicsAllocation *allocation) { preemptionCsrAllocation = allocation; }

    void requestThreadArbitrationPolicy(uint32_t requiredPolicy) { this->requiredThreadArbitrationPolicy = requiredPolicy; }
    void requestStallingPipeControlOnNextFlush() { stallingPipeControlOnNextFlushRequired = true; }

    virtual void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, OsContext &osContext, bool forcePowerSavingMode) = 0;
    MOCKABLE_VIRTUAL bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait);

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

    FlatBatchBufferHelper &getFlatBatchBufferHelper() { return *flatBatchBufferHelper.get(); }
    void overwriteFlatBatchBufferHelper(FlatBatchBufferHelper *newHelper) { flatBatchBufferHelper.reset(newHelper); }

    MOCKABLE_VIRTUAL void initProgrammingFlags();
    virtual void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo);

    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize);
    void allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap);
    void releaseIndirectHeap(IndirectHeap::Type heapType);

    virtual enum CommandStreamReceiverType getType() = 0;
    void setExperimentalCmdBuffer(std::unique_ptr<ExperimentalCommandBuffer> &&cmdBuffer);

    bool initializeTagAllocation();
    std::unique_lock<MutexType> obtainUniqueOwnership();

    KmdNotifyHelper *peekKmdNotifyHelper() {
        return kmdNotifyHelper.get();
    }

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    size_t defaultSshSize;

    void setDeviceIndex(uint32_t deviceIndex) { this->deviceIndex = deviceIndex; }
    uint32_t getDeviceIndex() const { return this->deviceIndex; }
    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    bool createAllocationForHostSurface(HostPtrSurface &surface, Device &device, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const { return 512; }

  protected:
    void cleanupResources();
    void setDisableL3Cache(bool val) {
        disableL3Cache = val;
    }

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;
    std::unique_ptr<FlatBatchBufferHelper> flatBatchBufferHelper;
    std::unique_ptr<ExperimentalCommandBuffer> experimentalCmdBuffer;
    std::unique_ptr<InternalAllocationStorage> internalAllocationStorage;
    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    MutexType ownershipMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;

    volatile uint32_t *tagAddress = nullptr;

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *scratchAllocation = nullptr;
    GraphicsAllocation *preemptionCsrAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    OSInterface *osInterface = nullptr;

    IndirectHeap *indirectHeap[IndirectHeap::NUM_TYPES];

    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};
    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};

    DispatchMode dispatchMode = DispatchMode::ImmediateDispatch;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;
    uint64_t totalMemoryUsed = 0u;

    uint32_t deviceIndex = 0u;
    // taskCount - # of tasks submitted
    uint32_t taskCount = 0;
    uint32_t lastSentL3Config = 0;
    uint32_t latestSentStatelessMocsConfig = 0;
    uint32_t lastSentNumGrfRequired = GrfConfig::DefaultGrfNumber;
    uint32_t requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    uint32_t lastSentThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    uint32_t requiredScratchSize = 0;

    int8_t lastSentCoherencyRequest = -1;
    int8_t lastMediaSamplerConfig = -1;

    bool isPreambleSent = false;
    bool isStateSipSent = false;
    bool GSBAFor32BitProgrammed = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool disableL3Cache = false;
    bool stallingPipeControlOnNextFlushRequired = false;
    bool timestampPacketWriteEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSpecialPipelineSelectMode = false;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT

/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_subcapture.h"
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

namespace NEO {
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
class ScratchSpaceController;
struct HwPerfCounter;
struct HwTimeStamps;
struct TimestampPacketStorage;

template <typename T1>
class TagAllocator;

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

    virtual FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual void flushBatchedSubmissions() = 0;

    virtual void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency);
    virtual void processResidency(ResidencyContainer &allocationsForResidency) {}
    virtual void processEviction();
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);

    void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize);

    MemoryManager *getMemoryManager() const;

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }

    MOCKABLE_VIRTUAL void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() const { return osInterface; };

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    GraphicsAllocation *getTagAllocation() const {
        return tagAllocation;
    }
    volatile uint32_t *getTagAddress() const { return tagAddress; }

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; };

    uint32_t peekTaskCount() const { return taskCount; }

    uint32_t peekTaskLevel() const { return taskLevel; }

    uint32_t peekLatestSentTaskCount() const { return latestSentTaskCount; }

    uint32_t peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void enableNTo1SubmissionModel() { this->nTo1SubmissionModelEnabled = true; }
    bool isNTo1SubmissionModelEnabled() const { return this->nTo1SubmissionModelEnabled; }
    void overrideDispatchPolicy(DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    void setMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }

    void setRequiredScratchSize(uint32_t newRequiredScratchSize);
    GraphicsAllocation *getScratchAllocation();
    GraphicsAllocation *getDebugSurfaceAllocation() const { return debugSurface; }
    GraphicsAllocation *allocateDebugSurface(size_t size);

    void setPreemptionCsrAllocation(GraphicsAllocation *allocation) { preemptionCsrAllocation = allocation; }

    void requestThreadArbitrationPolicy(uint32_t requiredPolicy) { this->requiredThreadArbitrationPolicy = requiredPolicy; }
    void requestStallingPipeControlOnNextFlush() { stallingPipeControlOnNextFlushRequired = true; }

    virtual void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) = 0;
    MOCKABLE_VIRTUAL bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait);

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

    FlatBatchBufferHelper &getFlatBatchBufferHelper() const { return *flatBatchBufferHelper; }
    void overwriteFlatBatchBufferHelper(FlatBatchBufferHelper *newHelper) { flatBatchBufferHelper.reset(newHelper); }

    MOCKABLE_VIRTUAL void initProgrammingFlags();
    virtual AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo);
    void programForAubSubCapture(bool wasActiveInPreviousEnqueue, bool isActive);
    virtual void addAubComment(const char *comment);

    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize);
    void allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap);
    void releaseIndirectHeap(IndirectHeap::Type heapType);

    virtual enum CommandStreamReceiverType getType() = 0;
    void setExperimentalCmdBuffer(std::unique_ptr<ExperimentalCommandBuffer> &&cmdBuffer);

    bool initializeTagAllocation();
    MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    size_t defaultSshSize;

    void setDeviceIndex(uint32_t deviceIndex) { this->deviceIndex = deviceIndex; }
    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const { return 512; }
    virtual void setupContext(OsContext &osContext) { this->osContext = &osContext; }
    OsContext &getOsContext() const { return *osContext; }

    TagAllocator<HwTimeStamps> *getEventTsAllocator();
    TagAllocator<HwPerfCounter> *getEventPerfCountAllocator();
    TagAllocator<TimestampPacketStorage> *getTimestampPacketAllocator();

    virtual cl_int expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);

    void setDisableL3Cache(bool val) {
        disableL3Cache = val;
    }
    bool isMultiOsContextCapable() const;

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    void blitWithHostPtr(Buffer &buffer, void *hostPtr, bool blocking, size_t bufferOffset, uint64_t copySize,
                         BlitterConstants::BlitWithHostPtrDirection copyDirection, CsrDependencies &csrDependencies,
                         const TimestampPacketContainer &outputTimestampPacket);
    virtual void blitBuffer(Buffer &dstBuffer, Buffer &srcBuffer, bool blocking, uint64_t dstOffset, uint64_t srcOffset,
                            uint64_t copySize, CsrDependencies &csrDependencies, const TimestampPacketContainer &outputTimestampPacket) = 0;

    ScratchSpaceController *getScratchSpaceController() const {
        return scratchSpaceController.get();
    }

  protected:
    void cleanupResources();

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;
    std::unique_ptr<FlatBatchBufferHelper> flatBatchBufferHelper;
    std::unique_ptr<ExperimentalCommandBuffer> experimentalCmdBuffer;
    std::unique_ptr<InternalAllocationStorage> internalAllocationStorage;
    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;
    std::unique_ptr<ScratchSpaceController> scratchSpaceController;
    std::unique_ptr<TagAllocator<HwTimeStamps>> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocator<HwPerfCounter>> perfCounterAllocator;
    std::unique_ptr<TagAllocator<TimestampPacketStorage>> timestampPacketAllocator;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    MutexType ownershipMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;

    volatile uint32_t *tagAddress = nullptr;

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *preemptionCsrAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    OSInterface *osInterface = nullptr;

    IndirectHeap *indirectHeap[IndirectHeap::NUM_TYPES];

    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};
    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};

    OsContext *osContext = nullptr;
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
    bool bindingTableBaseAddressRequired = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool disableL3Cache = false;
    bool stallingPipeControlOnNextFlushRequired = false;
    bool timestampPacketWriteEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSpecialPipelineSelectMode = false;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(bool withAubDump, ExecutionEnvironment &executionEnvironment);
} // namespace NEO

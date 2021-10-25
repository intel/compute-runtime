/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/aub_subcapture_status.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/csr_properties_flags.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/utilities/spinlock.h"

#include "pipe_control_args.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class AllocationsList;
class Device;
class ExecutionEnvironment;
class ExperimentalCommandBuffer;
class GmmPageTableMngr;
class GraphicsAllocation;
class HostPtrSurface;
class IndirectHeap;
class InternalAllocationStorage;
class LinearStream;
class MemoryManager;
class MultiGraphicsAllocation;
class OsContext;
class OSInterface;
class ScratchSpaceController;
class HwPerfCounter;
class HwTimeStamps;
class TagAllocatorBase;

template <typename TSize>
class TimestampPackets;

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
    CommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                          uint32_t rootDeviceIndex,
                          const DeviceBitfield deviceBitfield);
    virtual ~CommandStreamReceiver();

    virtual bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual bool flushBatchedSubmissions() = 0;
    MOCKABLE_VIRTUAL int submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);
    virtual void pollForCompletion() {}
    virtual void programHardwareContext(LinearStream &cmdStream) = 0;
    virtual size_t getCmdsSizeForHardwareContext() const = 0;

    void makeResident(MultiGraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency);
    virtual void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) {}
    virtual void processEviction();
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);

    MOCKABLE_VIRTUAL void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize);

    MemoryManager *getMemoryManager() const;

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }
    bool needsPageTableManager() const;

    void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage);
    MOCKABLE_VIRTUAL void waitForTaskCountAndCleanTemporaryAllocationList(uint32_t requiredTaskCount);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() const;
    ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; };

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    GraphicsAllocation *getTagAllocation() const {
        return tagAllocation;
    }
    MultiGraphicsAllocation *getTagsMultiAllocation() const {
        return tagsMultiAllocation;
    }
    MultiGraphicsAllocation &createTagsMultiAllocation();
    volatile uint32_t *getTagAddress() const { return tagAddress; }
    uint64_t getDebugPauseStateGPUAddress() const { return tagAllocation->getGpuAddress() + debugPauseStateAddressOffset; }

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; };

    uint32_t peekTaskCount() const { return taskCount; }

    uint32_t peekTaskLevel() const { return taskLevel; }
    FlushStamp obtainCurrentFlushStamp() const;

    uint32_t peekLatestSentTaskCount() const { return latestSentTaskCount; }

    uint32_t peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void enableNTo1SubmissionModel() { this->nTo1SubmissionModelEnabled = true; }
    bool isNTo1SubmissionModelEnabled() const { return this->nTo1SubmissionModelEnabled; }
    void overrideDispatchPolicy(DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    void setMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }
    bool getMediaVFEStateDirty() { return mediaVfeStateDirty; }

    void setGSBAStateDirty(bool dirty) { GSBAStateDirty = dirty; }
    bool getGSBAStateDirty() { return GSBAStateDirty; }

    void setRequiredScratchSizes(uint32_t newRequiredScratchSize, uint32_t newRequiredPrivateScratchSize);
    GraphicsAllocation *getScratchAllocation();
    GraphicsAllocation *getDebugSurfaceAllocation() const { return debugSurface; }
    GraphicsAllocation *allocateDebugSurface(size_t size);
    GraphicsAllocation *getPreemptionAllocation() const { return preemptionAllocation; }
    GraphicsAllocation *getGlobalFenceAllocation() const { return globalFenceAllocation; }
    GraphicsAllocation *getWorkPartitionAllocation() const { return workPartitionAllocation; }

    void requestStallingPipeControlOnNextFlush() { stallingPipeControlOnNextFlushRequired = true; }
    bool isStallingPipeControlOnNextFlushRequired() const { return stallingPipeControlOnNextFlushRequired; }

    virtual void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) = 0;
    virtual bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait);
    bool baseWaitFunction(volatile uint32_t *pollAddress, bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait);
    bool testTaskCountReady(volatile uint32_t *pollAddress, uint32_t taskCountToWait);
    virtual void downloadAllocations(){};

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

    FlatBatchBufferHelper &getFlatBatchBufferHelper() const { return *flatBatchBufferHelper; }
    void overwriteFlatBatchBufferHelper(FlatBatchBufferHelper *newHelper) { flatBatchBufferHelper.reset(newHelper); }

    MOCKABLE_VIRTUAL void initProgrammingFlags();
    virtual AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName);
    void programForAubSubCapture(bool wasActiveInPreviousEnqueue, bool isActive);
    virtual void addAubComment(const char *comment);

    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize);
    void allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap);
    void releaseIndirectHeap(IndirectHeap::Type heapType);

    virtual enum CommandStreamReceiverType getType() = 0;
    void setExperimentalCmdBuffer(std::unique_ptr<ExperimentalCommandBuffer> &&cmdBuffer);

    bool initializeTagAllocation();
    MOCKABLE_VIRTUAL bool createWorkPartitionAllocation(const Device &device);
    MOCKABLE_VIRTUAL bool createGlobalFenceAllocation();
    MOCKABLE_VIRTUAL bool createPreemptionAllocation();
    MOCKABLE_VIRTUAL bool createPerDssBackedBuffer(Device &device);
    MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    size_t defaultSshSize;
    bool canUse4GbHeaps = true;

    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const;
    virtual void setupContext(OsContext &osContext) { this->osContext = &osContext; }
    OsContext &getOsContext() const { return *osContext; }

    TagAllocatorBase *getEventTsAllocator();
    TagAllocatorBase *getEventPerfCountAllocator(const uint32_t tagSize);
    virtual TagAllocatorBase *getTimestampPacketAllocator() = 0;

    virtual bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);

    virtual bool isMultiOsContextCapable() const = 0;

    virtual MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const = 0;

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    virtual uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) = 0;

    virtual void flushTagUpdate() = 0;
    virtual void flushNonKernelTask(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isWaitOnEvents, bool isStartOfDispatch, bool isEndOfDispatch) = 0;
    virtual void updateTagFromWait() = 0;

    ScratchSpaceController *getScratchSpaceController() const {
        return scratchSpaceController.get();
    }

    void registerInstructionCacheFlush() {
        auto mutex = obtainUniqueOwnership();
        requiresInstructionCacheFlush = true;
    }

    bool isLocalMemoryEnabled() const { return localMemoryEnabled; }

    uint32_t getRootDeviceIndex() { return rootDeviceIndex; }

    virtual bool initDirectSubmission(Device &device, OsContext &osContext) {
        return true;
    }

    virtual bool isDirectSubmissionEnabled() const {
        return false;
    }

    virtual bool isBlitterDirectSubmissionEnabled() const {
        return false;
    }

    virtual void stopDirectSubmission() {}

    bool isStaticWorkPartitioningEnabled() const {
        return staticWorkPartitioningEnabled;
    }

    uint64_t getWorkPartitionAllocationGpuAddress() const;

    bool isRcs() const;

    virtual void initializeDefaultsForInternalEngine(){};

    virtual GraphicsAllocation *getClearColorAllocation() = 0;

    virtual void postInitFlagsSetup() = 0;

    bool isUsedNotifyEnableForPostSync() const {
        return useNotifyEnableForPostSync;
    }

    NEO::StreamProperties &getStreamProperties() {
        return this->streamProperties;
    }

    inline void setActivePartitions(uint32_t newPartitionCount) {
        activePartitions = newPartitionCount;
    }

    inline uint32_t getActivePartitions() const {
        return activePartitions;
    }

    std::unique_ptr<GmmPageTableMngr> pageTableManager;

  protected:
    void cleanupResources();
    void printDeviceIndex();
    void checkForNewResources(uint32_t submittedTaskCount, uint32_t allocationTaskCount, GraphicsAllocation &gfxAllocation);
    bool checkImplicitFlushForGpuIdle();
    MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainHostPtrSurfaceCreationLock();

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;
    std::unique_ptr<FlatBatchBufferHelper> flatBatchBufferHelper;
    std::unique_ptr<ExperimentalCommandBuffer> experimentalCmdBuffer;
    std::unique_ptr<InternalAllocationStorage> internalAllocationStorage;
    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;
    std::unique_ptr<ScratchSpaceController> scratchSpaceController;
    std::unique_ptr<TagAllocatorBase> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocatorBase> perfCounterAllocator;
    std::unique_ptr<TagAllocatorBase> timestampPacketAllocator;
    std::unique_ptr<Thread> userPauseConfirmation;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    MutexType ownershipMutex;
    MutexType hostPtrSurfaceCreationMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;
    StreamProperties streamProperties{};

    // offset for debug state must be 64 bytes, tag writes can use multiple dwords for multiple partitions
    const uint64_t debugPauseStateAddressOffset = MemoryConstants::cacheLineSize;
    uint64_t totalMemoryUsed = 0u;

    volatile uint32_t *tagAddress = nullptr;
    volatile DebugPauseState *debugPauseStateAddress = nullptr;
    SpinLock debugPauseStateLock;
    static void *asyncDebugBreakConfirmation(void *arg);
    std::function<void()> debugConfirmationFunction = []() { std::cin.get(); };

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *preemptionAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    GraphicsAllocation *perDssBackedBuffer = nullptr;
    GraphicsAllocation *clearColorAllocation = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;

    MultiGraphicsAllocation *tagsMultiAllocation = nullptr;

    IndirectHeap *indirectHeap[IndirectHeap::NUM_TYPES];
    OsContext *osContext = nullptr;

    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};
    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};
    // taskCount - # of tasks submitted
    std::atomic<uint32_t> taskCount{0};

    DispatchMode dispatchMode = DispatchMode::ImmediateDispatch;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;

    uint32_t lastSentL3Config = 0;
    uint32_t latestSentStatelessMocsConfig = 0;
    uint32_t lastSentNumGrfRequired = GrfConfig::DefaultGrfNumber;
    uint32_t requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    uint32_t lastSentThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
    uint64_t lastSentSliceCount = QueueSliceCount::defaultSliceCount;

    uint32_t requiredScratchSize = 0;
    uint32_t requiredPrivateScratchSize = 0;
    uint32_t lastAdditionalKernelExecInfo = AdditionalKernelExecInfo::NotSet;
    KernelExecutionType lastKernelExecutionType = KernelExecutionType::Default;
    MemoryCompressionState lastMemoryCompressionState = MemoryCompressionState::NotApplicable;
    uint32_t activePartitions = 1;
    uint32_t activePartitionsConfig = 1;

    const uint32_t rootDeviceIndex;
    const DeviceBitfield deviceBitfield;

    int8_t lastSentCoherencyRequest = -1;
    int8_t lastMediaSamplerConfig = -1;

    bool isPreambleSent = false;
    bool isStateSipSent = false;
    bool isEnginePrologueSent = false;
    bool isPerDssBackedBufferSent = false;
    bool GSBAFor32BitProgrammed = false;
    bool GSBAStateDirty = true;
    bool bindingTableBaseAddressRequired = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool stallingPipeControlOnNextFlushRequired = false;
    bool timestampPacketWriteEnabled = false;
    bool staticWorkPartitioningEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSpecialPipelineSelectMode = false;
    bool requiresInstructionCacheFlush = false;

    bool localMemoryEnabled = false;
    bool pageTableManagerInitialized = false;

    bool useNewResourceImplicitFlush = false;
    bool newResources = false;
    bool useGpuIdleImplicitFlush = false;
    bool lastSentUseGlobalAtomics = false;
    bool useNotifyEnableForPostSync = false;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(bool withAubDump,
                                                                  ExecutionEnvironment &executionEnvironment,
                                                                  uint32_t rootDeviceIndex,
                                                                  const DeviceBitfield deviceBitfield);
} // namespace NEO

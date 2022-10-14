/*
 * Copyright (C) 2018-2022 Intel Corporation
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
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/utilities/spinlock.h"
#include "shared/source/utilities/stackvec.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

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
class GmmHelper;
class TagAllocatorBase;
class LogicalStateHelper;

template <typename TSize>
class TimestampPackets;

template <typename T1>
class TagAllocator;

enum class DispatchMode {
    DeviceDefault = 0,          // default for given device
    ImmediateDispatch,          // everything is submitted to the HW immediately
    AdaptiveDispatch,           // dispatching is handled to async thread, which combines batch buffers basing on load (not implemented)
    BatchedDispatchWithCounter, // dispatching is batched, after n commands there is implicit flush (not implemented)
    BatchedDispatch             // dispatching is batched, explicit clFlush is required
};

class CommandStreamReceiver {
  public:
    static constexpr size_t startingResidencyContainerSize = 128;

    enum class SamplerCacheFlushState {
        samplerCacheFlushNotRequired,
        samplerCacheFlushBefore, // add sampler cache flush before Walker with redescribed image
        samplerCacheFlushAfter   // add sampler cache flush after Walker with redescribed image
    };

    using MutexType = std::recursive_mutex;
    using TimeType = std::chrono::high_resolution_clock::time_point;
    CommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                          uint32_t rootDeviceIndex,
                          const DeviceBitfield deviceBitfield);
    virtual ~CommandStreamReceiver();

    virtual SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual bool flushBatchedSubmissions() = 0;
    MOCKABLE_VIRTUAL SubmissionStatus submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);
    virtual void pollForCompletion() {}
    virtual void programHardwareContext(LinearStream &cmdStream) = 0;
    virtual size_t getCmdsSizeForHardwareContext() const = 0;

    void makeResident(MultiGraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency, bool clearAllocations);
    virtual bool processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) { return true; }
    virtual void processEviction();
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);

    MOCKABLE_VIRTUAL void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize);

    MemoryManager *getMemoryManager() const;

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }
    bool needsPageTableManager() const;

    MOCKABLE_VIRTUAL WaitStatus waitForTaskCount(uint32_t requiredTaskCount);
    WaitStatus waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage);
    MOCKABLE_VIRTUAL WaitStatus waitForTaskCountAndCleanTemporaryAllocationList(uint32_t requiredTaskCount);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() const;
    ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; }
    GmmHelper *peekGmmHelper() const;

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

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; }

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
    GraphicsAllocation *getKernelArgsBufferAllocation() const { return kernelArgsBufferAllocation; }

    void requestStallingCommandsOnNextFlush() { stallingCommandsOnNextFlushRequired = true; }
    bool isStallingCommandsOnNextFlushRequired() const { return stallingCommandsOnNextFlushRequired; }

    virtual WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) = 0;
    virtual WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait);
    WaitStatus baseWaitFunction(volatile uint32_t *pollAddress, const WaitParams &params, uint32_t taskCountToWait);
    MOCKABLE_VIRTUAL bool testTaskCountReady(volatile uint32_t *pollAddress, uint32_t taskCountToWait);
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
    virtual void createKernelArgsBufferAllocation() = 0;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    bool isLatestTaskCountFlushed() {
        return this->peekLatestFlushedTaskCount() == this->peekTaskCount();
    }

    size_t defaultSshSize = 0u;
    bool canUse4GbHeaps = true;

    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    AllocationsList &getDeferredAllocations();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const;
    virtual void fillReusableAllocationsList();
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
    void setLatestFlushedTaskCount(uint32_t latestFlushedTaskCount) {
        this->latestFlushedTaskCount = latestFlushedTaskCount;
    }

    virtual std::optional<uint32_t> flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) = 0;

    virtual void flushTagUpdate() = 0;
    virtual void updateTagFromWait() = 0;
    virtual bool isUpdateTagFromWaitEnabled() = 0;

    ScratchSpaceController *getScratchSpaceController() const {
        return scratchSpaceController.get();
    }

    void downloadAllocation(GraphicsAllocation &gfxAllocation);

    void registerInstructionCacheFlush() {
        auto mutex = obtainUniqueOwnership();
        requiresInstructionCacheFlush = true;
    }

    bool isLocalMemoryEnabled() const { return localMemoryEnabled; }

    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }

    void startControllingDirectSubmissions();

    bool isAnyDirectSubmissionEnabled() {
        return this->isDirectSubmissionEnabled() || isBlitterDirectSubmissionEnabled();
    }

    virtual bool initDirectSubmission() {
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

    MOCKABLE_VIRTUAL bool isRcs() const;

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

    bool skipResourceCleanup() const;

    inline bool isProgramActivePartitionConfigRequired() const {
        return this->isDirectSubmissionEnabled() ? false : this->activePartitionsConfig != this->activePartitions;
    }

    std::unique_ptr<GmmPageTableMngr> pageTableManager;

    inline uint32_t getPostSyncWriteOffset() const {
        return postSyncWriteOffset;
    }

    inline bool isMultiTileOperationEnabled() const {
        return (activePartitions > 1) && staticWorkPartitioningEnabled;
    }

    virtual void programComputeBarrierCommand(LinearStream &cmdStream) = 0;
    virtual size_t getCmdsSizeForComputeBarrierCommand() const = 0;

    const HardwareInfo &peekHwInfo() const;
    const RootDeviceEnvironment &peekRootDeviceEnvironment() const;

    MOCKABLE_VIRTUAL bool isGpuHangDetected() const;
    MOCKABLE_VIRTUAL bool checkGpuHangDetected(TimeType currentTime, TimeType &lastHangCheckTime) const;

    uint64_t getCompletionAddress() const {
        uint64_t completionFenceAddress = castToUint64(const_cast<uint32_t *>(getTagAddress()));
        if (completionFenceAddress == 0) {
            return 0;
        }
        completionFenceAddress += completionFenceOffset;
        return completionFenceAddress;
    }

    uint32_t getCompletionValue(const GraphicsAllocation &gfxAllocation);
    DispatchMode getDispatchMode() const {
        return this->dispatchMode;
    }

    LogicalStateHelper *getLogicalStateHelper() const;

    bool getPreambleSetFlag() const {
        return isPreambleSent;
    }

    void setPreambleSetFlag(bool value) {
        isPreambleSent = value;
    }

    PreemptionMode getPreemptionMode() const {
        return lastPreemptionMode;
    }

    void setPreemptionMode(PreemptionMode value) {
        lastPreemptionMode = value;
    }

    virtual void initializeDeviceWithFirstSubmission() = 0;

    uint32_t getNumClients() {
        return this->numClients.load();
    }
    void registerClient() {
        this->numClients++;
    }
    void unregisterClient() {
        this->numClients--;
    }

    bool getDcFlushSupport() const {
        return dcFlushSupport;
    }

    bool getDcFlushRequired(bool externalCondition) const {
        return externalCondition ? dcFlushSupport : false;
    }

  protected:
    void cleanupResources();
    void printDeviceIndex();
    void checkForNewResources(uint32_t submittedTaskCount, uint32_t allocationTaskCount, GraphicsAllocation &gfxAllocation);
    bool checkImplicitFlushForGpuIdle();
    void downloadTagAllocation(uint32_t taskCountToWait);
    void printTagAddressContent(uint32_t taskCountToWait, int64_t waitTimeout, bool start);
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainHostPtrSurfaceCreationLock();

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
    std::unique_ptr<LogicalStateHelper> logicalStateHelper;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    MutexType ownershipMutex;
    MutexType hostPtrSurfaceCreationMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;
    StreamProperties streamProperties{};
    FrontEndPropertiesSupport feSupportFlags{};
    PipelineSelectPropertiesSupport pipelineSupportFlags{};

    // offset for debug state is 1kbyte, tag writes can use multiple offsets for multiple partitions and each offset can vary per platform
    const uint64_t debugPauseStateAddressOffset = MemoryConstants::kiloByte;
    uint64_t totalMemoryUsed = 0u;

    volatile uint32_t *tagAddress = nullptr;
    volatile DebugPauseState *debugPauseStateAddress = nullptr;
    SpinLock debugPauseStateLock;
    static void *asyncDebugBreakConfirmation(void *arg);
    std::function<void()> debugConfirmationFunction = []() { std::cin.get(); };
    std::function<void(GraphicsAllocation &)> downloadAllocationImpl;

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *preemptionAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    GraphicsAllocation *perDssBackedBuffer = nullptr;
    GraphicsAllocation *clearColorAllocation = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    GraphicsAllocation *kernelArgsBufferAllocation = nullptr;

    MultiGraphicsAllocation *tagsMultiAllocation = nullptr;

    IndirectHeap *indirectHeap[IndirectHeap::Type::NUM_TYPES];
    OsContext *osContext = nullptr;
    uint32_t *completionFenceValuePointer = nullptr;

    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};
    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};
    // taskCount - # of tasks submitted
    std::atomic<uint32_t> taskCount{0};

    std::atomic<uint32_t> numClients = 0u;

    DispatchMode dispatchMode = DispatchMode::ImmediateDispatch;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;

    std::chrono::microseconds gpuHangCheckPeriod{500'000};
    uint32_t lastSentL3Config = 0;
    uint32_t latestSentStatelessMocsConfig = 0;
    uint64_t lastSentSliceCount = QueueSliceCount::defaultSliceCount;

    uint32_t requiredScratchSize = 0;
    uint32_t requiredPrivateScratchSize = 0;
    uint32_t lastAdditionalKernelExecInfo = AdditionalKernelExecInfo::NotSet;
    KernelExecutionType lastKernelExecutionType = KernelExecutionType::Default;
    MemoryCompressionState lastMemoryCompressionState = MemoryCompressionState::NotApplicable;
    uint32_t activePartitions = 1;
    uint32_t activePartitionsConfig = 1;
    uint32_t postSyncWriteOffset = 0;
    uint32_t completionFenceOffset = 0;
    uint32_t completionFenceValue = 0;

    const uint32_t rootDeviceIndex;
    const DeviceBitfield deviceBitfield;

    int8_t lastMediaSamplerConfig = -1;

    bool isPreambleSent = false;
    bool isStateSipSent = false;
    bool isEnginePrologueSent = false;
    bool isPerDssBackedBufferSent = false;
    bool GSBAFor32BitProgrammed = false;
    bool GSBAStateDirty = true;
    bool bindingTableBaseAddressRequired = false;
    bool heapStorageReqiuresRecyclingTag = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool stallingCommandsOnNextFlushRequired = false;
    bool timestampPacketWriteEnabled = false;
    bool staticWorkPartitioningEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSystolicPipelineSelectMode = false;
    bool requiresInstructionCacheFlush = false;

    bool localMemoryEnabled = false;
    bool pageTableManagerInitialized = false;

    bool useNewResourceImplicitFlush = false;
    bool newResources = false;
    bool useGpuIdleImplicitFlush = false;
    bool lastSentUseGlobalAtomics = false;
    bool useNotifyEnableForPostSync = false;
    bool dcFlushSupport = false;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(bool withAubDump,
                                                                  ExecutionEnvironment &executionEnvironment,
                                                                  uint32_t rootDeviceIndex,
                                                                  const DeviceBitfield deviceBitfield);
} // namespace NEO

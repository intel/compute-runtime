/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/blit_properties_container.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/options.h"
#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/source/utilities/spinlock.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>

namespace aub_stream {
struct AllocationParams;
} // namespace aub_stream

namespace NEO {
enum class AllocationType;
enum class DebugPauseState : uint32_t;
struct BatchBuffer;
struct DispatchBcsFlags;
struct DispatchFlags;
struct HardwareInfo;
struct ImmediateDispatchFlags;
struct WaitParams;
class SubmissionAggregator;
class FlushStampTracker;
class Thread;
class FlatBatchBufferHelper;
class AllocationsList;
class Device;
class ExecutionEnvironment;
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
class TimestampPacketContainer;
class KmdNotifyHelper;
class GfxCoreHelper;
class ProductHelper;
class ReleaseHelper;
enum class WaitStatus;
struct AubSubCaptureStatus;

template <typename TSize, uint32_t packetCount>
class TimestampPackets;

template <typename T1>
class TagAllocator;
class TagNodeBase;

enum class DispatchMode {
    deviceDefault = 0,          // default for given device
    immediateDispatch,          // everything is submitted to the HW immediately
    adaptiveDispatch,           // dispatching is handled to async thread, which combines batch buffers basing on load (not implemented)
    batchedDispatchWithCounter, // dispatching is batched, after n commands there is implicit flush (not implemented)
    batchedDispatch             // dispatching is batched, explicit clFlush is required
};

class CommandStreamReceiver : NEO::NonCopyableAndNonMovableClass {
  public:
    static constexpr size_t startingResidencyContainerSize = 128;

    enum class SamplerCacheFlushState {
        samplerCacheFlushNotRequired,
        samplerCacheFlushBefore, // add sampler cache flush before Walker with redescribed image
        samplerCacheFlushAfter   // add sampler cache flush after Walker with redescribed image
    };

    using MutexType = std::recursive_mutex;
    using TimeType = std::chrono::high_resolution_clock::time_point;

    CommandStreamReceiver() = delete;

    CommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                          uint32_t rootDeviceIndex,
                          const DeviceBitfield deviceBitfield);

    virtual ~CommandStreamReceiver();

    virtual SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                      const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                                      TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual CompletionStamp flushBcsTask(LinearStream &commandStream, size_t commandStreamStart, const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) = 0;
    virtual CompletionStamp flushImmediateTask(LinearStream &immediateCommandStream, size_t immediateCommandStreamStart,
                                               ImmediateDispatchFlags &dispatchFlags, Device &device) = 0;

    virtual CompletionStamp flushImmediateTaskStateless(LinearStream &immediateCommandStream, size_t immediateCommandStreamStart,
                                                        ImmediateDispatchFlags &dispatchFlags, Device &device) = 0;
    virtual SubmissionStatus sendRenderStateCacheFlush() = 0;

    virtual bool flushBatchedSubmissions() = 0;
    MOCKABLE_VIRTUAL SubmissionStatus submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);
    void pollForCompletion() { pollForCompletion(false); }
    virtual void pollForAubCompletion(){};
    virtual void pollForCompletion(bool skipTaskCountCheck) {}
    virtual void programHardwareContext(LinearStream &cmdStream) = 0;
    virtual size_t getCmdsSizeForHardwareContext() const = 0;

    void makeResident(MultiGraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency, bool clearAllocations);
    virtual SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId);
    virtual void processEviction();
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);

    MOCKABLE_VIRTUAL void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize);

    MemoryManager *getMemoryManager() const;

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();
    PrivateAllocsToReuseContainer &getOwnedPrivateAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }
    bool needsPageTableManager() const;

    MOCKABLE_VIRTUAL WaitStatus waitForTaskCount(TaskCountType requiredTaskCount);
    WaitStatus waitForTaskCountAndCleanAllocationList(TaskCountType requiredTaskCount, uint32_t allocationUsage);
    MOCKABLE_VIRTUAL WaitStatus waitForTaskCountAndCleanTemporaryAllocationList(TaskCountType requiredTaskCount);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() const;
    ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; }
    GmmHelper *peekGmmHelper() const;
    DeviceBitfield peekDeviceBitfield() const { return deviceBitfield; }

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    GraphicsAllocation *getTagAllocation() const {
        return tagAllocation;
    }
    MultiGraphicsAllocation *getTagsMultiAllocation() const {
        return tagsMultiAllocation;
    }
    MultiGraphicsAllocation &createTagsMultiAllocation();

    TaskCountType getNextBarrierCount() { return this->barrierCount.fetch_add(1u); }
    TaskCountType peekBarrierCount() const { return this->barrierCount.load(); }
    volatile TagAddressType *getTagAddress() const { return tagAddress; }
    volatile TagAddressType *getBarrierCountTagAddress() const { return this->barrierCountTagAddress; }
    uint64_t getBarrierCountGpuAddress() const;
    uint64_t getDebugPauseStateGPUAddress() const;

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; }

    TaskCountType peekTaskCount() const { return taskCount; }

    TaskCountType peekTaskLevel() const { return taskLevel; }
    FlushStamp obtainCurrentFlushStamp() const;

    TaskCountType peekLatestSentTaskCount() const { return latestSentTaskCount; }

    TaskCountType peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void enableNTo1SubmissionModel() { this->nTo1SubmissionModelEnabled = true; }
    bool isNTo1SubmissionModelEnabled() const { return this->nTo1SubmissionModelEnabled; }
    void overrideDispatchPolicy(DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    void setMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }
    bool getMediaVFEStateDirty() const { return mediaVfeStateDirty; }

    void setGSBAStateDirty(bool dirty) { gsbaStateDirty = dirty; }
    bool getGSBAStateDirty() const { return gsbaStateDirty; }

    void setStateComputeModeDirty(bool dirty) { stateComputeModeDirty = dirty; }
    bool getStateComputeModeDirty() const { return stateComputeModeDirty; }

    void setBtdCommandDirty(bool dirty) { btdCommandDirty = dirty; }
    bool getBtdCommandDirty() const { return btdCommandDirty; }
    bool isRayTracingStateProgramingNeeded(Device &device) const;

    void setRequiredScratchSizes(uint32_t newRequiredScratchSlot0Size, uint32_t newRequiredPrivateScratchSlot1Size);
    GraphicsAllocation *getScratchAllocation();
    GraphicsAllocation *getDebugSurfaceAllocation() const {
        if (primaryCsr) {
            return primaryCsr->getDebugSurfaceAllocation();
        }
        return debugSurface;
    }
    GraphicsAllocation *allocateDebugSurface(size_t size);
    GraphicsAllocation *getPreemptionAllocation() const {
        if (primaryCsr) {
            return primaryCsr->getPreemptionAllocation();
        }
        return preemptionAllocation;
    }
    GraphicsAllocation *getGlobalFenceAllocation() const {
        if (primaryCsr) {
            return primaryCsr->getGlobalFenceAllocation();
        }

        return globalFenceAllocation;
    }
    GraphicsAllocation *getWorkPartitionAllocation() const {
        if (primaryCsr) {
            return primaryCsr->getWorkPartitionAllocation();
        }
        return workPartitionAllocation;
    }

    GraphicsAllocation *getGlobalStatelessHeapAllocation() const {
        if (primaryCsr) {
            return primaryCsr->getGlobalStatelessHeapAllocation();
        }

        return globalStatelessHeapAllocation;
    }

    virtual WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) = 0;
    virtual WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait);
    WaitStatus baseWaitFunction(volatile TagAddressType *pollAddress, const WaitParams &params, TaskCountType taskCountToWait);
    MOCKABLE_VIRTUAL bool testTaskCountReady(volatile TagAddressType *pollAddress, TaskCountType taskCountToWait);
    void downloadAllocations(bool blockingWait) { downloadAllocations(blockingWait, this->latestFlushedTaskCount); };
    virtual void downloadAllocations(bool blockingWait, TaskCountType taskCount){};
    virtual void removeDownloadAllocation(GraphicsAllocation *alloc){};

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

    FlatBatchBufferHelper &getFlatBatchBufferHelper() const { return *flatBatchBufferHelper; }
    void overwriteFlatBatchBufferHelper(FlatBatchBufferHelper *newHelper);

    MOCKABLE_VIRTUAL void initProgrammingFlags();
    virtual AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName);
    void programForAubSubCapture(bool wasActiveInPreviousEnqueue, bool isActive);
    virtual void addAubComment(const char *comment);

    IndirectHeap &getIndirectHeap(IndirectHeapType heapType, size_t minRequiredSize);
    void allocateHeapMemory(IndirectHeapType heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap);
    void releaseIndirectHeap(IndirectHeapType heapType);
    void *getIndirectHeapCurrentPtr(IndirectHeapType heapType) const;

    virtual enum CommandStreamReceiverType getType() const = 0;

    bool initializeTagAllocation();
    MOCKABLE_VIRTUAL bool createWorkPartitionAllocation(const Device &device);
    MOCKABLE_VIRTUAL bool createGlobalFenceAllocation();
    MOCKABLE_VIRTUAL bool createPreemptionAllocation();
    MOCKABLE_VIRTUAL bool createPerDssBackedBuffer(Device &device);
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    bool isLatestTaskCountFlushed() {
        return this->peekLatestFlushedTaskCount() == this->peekTaskCount();
    }

    size_t defaultSshSize = 0u;

    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    AllocationsList &getDeferredAllocations();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const;
    virtual void fillReusableAllocationsList();
    virtual void setupContext(OsContext &osContext) { this->osContext = &osContext; }
    OsContext &getOsContext() const { return *osContext; }
    uint8_t getUmdPowerHintValue() const;
    bool initializeResources(bool allocateInterrupt, const PreemptionMode preemptionMode);
    TagAllocatorBase *getEventTsAllocator();
    TagAllocatorBase *getEventPerfCountAllocator(const uint32_t tagSize);
    virtual TagAllocatorBase *getTimestampPacketAllocator() = 0;
    virtual std::unique_ptr<TagAllocatorBase> createMultiRootDeviceTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices) = 0;

    virtual bool expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);
    MOCKABLE_VIRTUAL bool writeMemory(GraphicsAllocation &gfxAllocation) { return writeMemory(gfxAllocation, false, 0, 0); }
    virtual bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) { return false; }
    virtual void writeMemoryAub(aub_stream::AllocationParams &allocationParams){};
    virtual void initializeEngine(){};

    virtual bool isMultiOsContextCapable() const = 0;

    virtual MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired) const = 0;

    void setLatestSentTaskCount(TaskCountType latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }
    void setLatestFlushedTaskCount(TaskCountType latestFlushedTaskCount) {
        this->latestFlushedTaskCount = latestFlushedTaskCount;
    }

    virtual TaskCountType flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, Device &device) = 0;

    virtual SubmissionStatus flushTagUpdate() = 0;
    virtual void updateTagFromWait() = 0;
    virtual bool isUpdateTagFromWaitEnabled() = 0;
    virtual void flushMonitorFence(bool notifyKmd){};
    virtual bool isTlbFlushRequiredForStateCacheFlush();

    ScratchSpaceController *getScratchSpaceController() const {
        return scratchSpaceController.get();
    }

    ScratchSpaceController *getPrimaryScratchSpaceController() const {
        if (primaryCsr) {
            return primaryCsr->getScratchSpaceController();
        }
        return getScratchSpaceController();
    }

    void downloadAllocation(GraphicsAllocation &gfxAllocation);

    bool isInstructionCacheFlushRequired() const {
        return requiresInstructionCacheFlush;
    }

    void setInstructionCacheFlushed() {
        requiresInstructionCacheFlush = false;
    }

    void registerInstructionCacheFlush() {
        auto mutex = obtainUniqueOwnership();
        requiresInstructionCacheFlush = true;
    }

    bool isLocalMemoryEnabled() const { return localMemoryEnabled; }

    uint32_t getRootDeviceIndex() const { return rootDeviceIndex; }

    MOCKABLE_VIRTUAL void startControllingDirectSubmissions();

    MOCKABLE_VIRTUAL bool isAnyDirectSubmissionEnabled() const {
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

    virtual bool directSubmissionRelaxedOrderingEnabled() const {
        return false;
    }

    virtual uint32_t getDirectSubmissionRelaxedOrderingQueueDepth() const { return 0; }

    virtual bool isKmdWaitOnTaskCountAllowed() const {
        return false;
    }

    virtual void stopDirectSubmission(bool blocking, bool needsLock) {}

    virtual QueueThrottle getLastDirectSubmissionThrottle() = 0;

    bool isStaticWorkPartitioningEnabled() const {
        return staticWorkPartitioningEnabled;
    }

    uint64_t getWorkPartitionAllocationGpuAddress() const;

    MOCKABLE_VIRTUAL bool isRcs() const;

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

    inline uint32_t getImmWritePostSyncWriteOffset() const {
        return immWritePostSyncWriteOffset;
    }

    inline uint32_t getTimeStampPostSyncWriteOffset() const {
        return timeStampPostSyncWriteOffset;
    }

    inline bool isMultiTileOperationEnabled() const {
        return (activePartitions > 1) && staticWorkPartitioningEnabled;
    }

    virtual void programComputeBarrierCommand(LinearStream &cmdStream) = 0;
    virtual size_t getCmdsSizeForComputeBarrierCommand() const = 0;

    virtual void programStallingCommandsForBarrier(LinearStream &cmdStream, TimestampPacketContainer *barrierTimestampPacketNodes, const bool isDcFlushRequired) = 0;

    const HardwareInfo &peekHwInfo() const;
    const RootDeviceEnvironment &peekRootDeviceEnvironment() const;

    const GfxCoreHelper &getGfxCoreHelper() const;
    const ProductHelper &getProductHelper() const;
    const ReleaseHelper *getReleaseHelper() const;

    MOCKABLE_VIRTUAL bool isGpuHangDetected() const;
    MOCKABLE_VIRTUAL bool checkGpuHangDetected(TimeType currentTime, TimeType &lastHangCheckTime) const;

    uint64_t getCompletionAddress() const;

    TaskCountType getCompletionValue(const GraphicsAllocation &gfxAllocation);
    DispatchMode getDispatchMode() const {
        return this->dispatchMode;
    }

    bool getPreambleSetFlag() const {
        return isPreambleSent;
    }

    void setPreambleSetFlag(bool value) {
        isPreambleSent = value;
    }

    bool getSipSentFlag() const {
        return isStateSipSent;
    }

    void setSipSentFlag(bool value) {
        isStateSipSent = value;
    }

    PreemptionMode getPreemptionMode() const {
        return lastPreemptionMode;
    }

    void setPreemptionMode(PreemptionMode value) {
        lastPreemptionMode = value;
    }
    bool csrSurfaceProgrammed() const {
        return csrSurfaceProgrammingDone;
    }
    void setCsrSurfaceProgrammed(bool value) {
        csrSurfaceProgrammingDone = value;
    }

    virtual SubmissionStatus initializeDeviceWithFirstSubmission(Device &device) = 0;

    uint32_t getNumClients() const {
        return this->numClients.load();
    }
    void registerClient(void *client);
    void unregisterClient(void *client);

    bool getDcFlushSupport() const {
        return dcFlushSupport;
    }

    bool getDcFlushRequired(bool externalCondition) const {
        return externalCondition ? dcFlushSupport : false;
    }

    bool getHeaplessStateInitEnabled() const {
        return heaplessStateInitEnabled;
    }

    bool isTbxMode() const;
    bool ensureTagAllocationForRootDeviceIndex(uint32_t rootDeviceIndex);

    L1CachePolicy *getStoredL1CachePolicy() {
        return &l1CachePolicyData;
    }
    void createGlobalStatelessHeap();
    IndirectHeap *getGlobalStatelessHeap() {
        if (primaryCsr) {
            return primaryCsr->getGlobalStatelessHeap();
        }
        return globalStatelessHeap.get();
    }

    bool isRecyclingTagForHeapStorageRequired() const { return heapStorageRequiresRecyclingTag; }

    virtual bool waitUserFenceSupported() { return false; }
    virtual bool waitUserFence(TaskCountType waitValue, uint64_t hostAddress, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) { return false; }
    void setPrimaryCsr(CommandStreamReceiver *primaryCsr) {
        this->primaryCsr = primaryCsr;
    }
    CommandStreamReceiver *getPrimaryCsr() const { return primaryCsr; }

    void requestPreallocation();
    void releasePreallocationRequest();
    void preallocateAllocation(AllocationType type, size_t size);
    void preallocateCommandBuffer();
    void preallocateInternalHeap();

    bool isInitialized() const {
        return this->resourcesInitialized;
    }

    MOCKABLE_VIRTUAL bool getAcLineConnected(bool updateStatus) const {
        if (updateStatus) {
            this->kmdNotifyHelper->updateAcLineStatus();
        }
        return this->kmdNotifyHelper->getAcLineConnected();
    }

    uint32_t getRequiredScratchSlot0Size() { return requiredScratchSlot0Size; }
    uint32_t getRequiredScratchSlot1Size() { return requiredScratchSlot1Size; }
    virtual bool submitDependencyUpdate(TagNodeBase *tag) = 0;

    MOCKABLE_VIRTUAL bool isBusy() {
        return !testTaskCountReady(getTagAddress(), this->taskCount);
    }

    bool isBusyWithoutHang(TimeType &lastHangCheckTime) {
        return isBusy() && !this->checkGpuHangDetected(std::chrono::high_resolution_clock::now(), lastHangCheckTime);
    }

    bool canUse4GbHeaps() const {
        return this->use4GbHeaps;
    }

    void ensurePrimaryCsrInitialized(Device &device);

    bool enqueueWaitForPagingFence(uint64_t pagingFenceValue);
    virtual void unblockPagingFenceSemaphore(uint64_t pagingFenceValue) {}
    MOCKABLE_VIRTUAL void drainPagingFenceQueue();
    bool isLatestFlushIsTaskCountUpdateOnly() const { return latestFlushIsTaskCountUpdateOnly; }

  protected:
    virtual CompletionStamp flushTaskHeapless(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                                              TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual CompletionStamp flushTaskHeapful(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                             const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                                             TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    void cleanupResources();
    void printDeviceIndex();
    void checkForNewResources(TaskCountType submittedTaskCount, TaskCountType allocationTaskCount, GraphicsAllocation &gfxAllocation);
    bool checkImplicitFlushForGpuIdle();
    void downloadTagAllocation(TaskCountType taskCountToWait);
    void printTagAddressContent(TaskCountType taskCountToWait, int64_t waitTimeout, bool start);
    virtual void addToEvictionContainer(GraphicsAllocation &gfxAllocation);

    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainHostPtrSurfaceCreationLock();

    std::vector<void *> registeredClients;

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;
    std::unique_ptr<FlatBatchBufferHelper> flatBatchBufferHelper;
    std::unique_ptr<InternalAllocationStorage> internalAllocationStorage;
    std::atomic<uint32_t> preallocatedAmount{0};
    std::atomic<uint32_t> requestedPreallocationsAmount{0};

    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;
    std::unique_ptr<ScratchSpaceController> scratchSpaceController;
    std::unique_ptr<TagAllocatorBase> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocatorBase> perfCounterAllocator;
    std::unique_ptr<TagAllocatorBase> timestampPacketAllocator;
    std::unique_ptr<Thread> userPauseConfirmation;
    std::unique_ptr<IndirectHeap> globalStatelessHeap;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    PrivateAllocsToReuseContainer ownedPrivateAllocations;

    MutexType ownershipMutex;
    MutexType hostPtrSurfaceCreationMutex;
    MutexType registeredClientsMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;
    StreamProperties streamProperties{};
    FrontEndPropertiesSupport feSupportFlags{};
    PipelineSelectPropertiesSupport pipelineSupportFlags{};
    StateBaseAddressPropertiesSupport sbaSupportFlags{};
    L1CachePolicy l1CachePolicyData{};

    uint64_t totalMemoryUsed = 0u;

    volatile TagAddressType *tagAddress = nullptr;
    volatile TagAddressType *barrierCountTagAddress = nullptr;
    volatile DebugPauseState *debugPauseStateAddress = nullptr;
    SpinLock debugPauseStateLock;
    static void *asyncDebugBreakConfirmation(void *arg);
    static std::function<void()> debugConfirmationFunction;
    std::function<void(GraphicsAllocation &)> downloadAllocationImpl;

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *preemptionAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    GraphicsAllocation *perDssBackedBuffer = nullptr;
    GraphicsAllocation *clearColorAllocation = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    GraphicsAllocation *globalStatelessHeapAllocation = nullptr;

    MultiGraphicsAllocation *tagsMultiAllocation = nullptr;

    IndirectHeap *indirectHeap[IndirectHeapType::numTypes];
    OsContext *osContext = nullptr;
    CommandStreamReceiver *primaryCsr = nullptr;
    TaskCountType *completionFenceValuePointer = nullptr;

    std::atomic<TaskCountType> barrierCount{0};
    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<TaskCountType> taskLevel{0};
    std::atomic<TaskCountType> latestSentTaskCount{0};
    std::atomic<TaskCountType> latestFlushedTaskCount{0};
    // taskCount - # of tasks submitted
    std::atomic<TaskCountType> taskCount{0};

    std::atomic<uint32_t> numClients = 0u;

    DispatchMode dispatchMode = DispatchMode::immediateDispatch;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;

    std::chrono::microseconds gpuHangCheckPeriod{CommonConstants::gpuHangCheckTimeInUS};
    uint32_t lastSentL3Config = 0;
    uint32_t latestSentStatelessMocsConfig;
    uint64_t lastSentSliceCount;

    uint32_t requiredScratchSlot0Size = 0;
    uint32_t requiredScratchSlot1Size = 0;
    uint32_t lastAdditionalKernelExecInfo;
    KernelExecutionType lastKernelExecutionType = KernelExecutionType::defaultType;
    MemoryCompressionState lastMemoryCompressionState = MemoryCompressionState::notApplicable;
    uint32_t activePartitions = 1;
    uint32_t activePartitionsConfig = 1;
    uint32_t immWritePostSyncWriteOffset = 0;
    uint32_t timeStampPostSyncWriteOffset = 0;
    TaskCountType completionFenceValue = 0;

    const uint32_t rootDeviceIndex;
    const DeviceBitfield deviceBitfield;

    int8_t lastMediaSamplerConfig = -1;

    bool isPreambleSent = false;
    bool isStateSipSent = false;
    bool isEnginePrologueSent = false;
    bool areExceptionsSent = false;
    bool isPerDssBackedBufferSent = false;
    bool gsbaFor32BitProgrammed = false;
    bool gsbaStateDirty = true;
    bool bindingTableBaseAddressRequired = false;
    bool heapStorageRequiresRecyclingTag = false;
    bool mediaVfeStateDirty = true;
    bool stateComputeModeDirty = true;
    bool btdCommandDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool timestampPacketWriteEnabled = false;
    bool staticWorkPartitioningEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSystolicPipelineSelectMode = false;
    bool requiresInstructionCacheFlush = false;
    bool requiresDcFlush = false;
    bool pushAllocationsForMakeResident = true;

    bool localMemoryEnabled = false;
    bool pageTableManagerInitialized = false;

    bool useNewResourceImplicitFlush = false;
    bool newResources = false;
    bool useGpuIdleImplicitFlush = false;
    bool useNotifyEnableForPostSync = false;
    bool dcFlushSupport = false;
    bool forceSkipResourceCleanupRequired = false;
    bool resourcesInitialized = false;
    bool heaplessStateInitialized = false;
    bool heaplessStateInitEnabled = false;
    bool doubleSbaWa = false;
    bool dshSupported = false;
    bool heaplessModeEnabled = false;
    bool use4GbHeaps = true;
    bool csrSurfaceProgrammingDone = false;
    bool latestFlushIsTaskCountUpdateOnly = false;
};

static_assert(NEO::NonCopyableAndNonMovable<CommandStreamReceiver>);

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(bool withAubDump,
                                                                  ExecutionEnvironment &executionEnvironment,
                                                                  uint32_t rootDeviceIndex,
                                                                  const DeviceBitfield deviceBitfield);
} // namespace NEO

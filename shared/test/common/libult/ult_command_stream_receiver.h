/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/aub_subcapture_status.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <map>
#include <optional>

namespace NEO {
class GmmPageTableMngr;

struct WaitUserFenceParams {
    uint64_t latestWaitedAddress = 0;
    uint64_t latestWaitedValue = 0;
    int64_t latestWaitedTimeout = 0;
    uint32_t callCount = 0;
    uint32_t externalInterruptId = NEO::InterruptId::notUsed;
    GraphicsAllocation *latestAllocForInterruptWait = nullptr;
    bool userInterrupt = false;
    bool forceRetStatusEnabled = false;
    bool forceRetStatusValue = true;
};

struct WriteMemoryParams {
    GraphicsAllocation *latestGfxAllocation = nullptr;
    uint64_t latestGpuVaChunkOffset = 0;
    size_t latestChunkSize = 0;
    uint32_t totalCallCount = 0;
    uint32_t chunkWriteCallCount = 0;
    bool latestChunkedMode = false;
};

template <typename GfxFamily>
class UltCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily> {
    using BaseClass = CommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::addPipeControlBefore3dState;
    using BaseClass::bcsRelaxedOrderingAllowed;
    using BaseClass::blitterDirectSubmission;
    using BaseClass::checkPlatformSupportsGpuIdleImplicitFlush;
    using BaseClass::checkPlatformSupportsNewResourceImplicitFlush;
    using BaseClass::createScratchSpaceController;
    using BaseClass::csrSizeRequestFlags;
    using BaseClass::dcFlushSupport;
    using BaseClass::directSubmission;
    using BaseClass::dshState;
    using BaseClass::flushTaskHeapful;
    using BaseClass::flushTaskHeapless;
    using BaseClass::getCmdSizeForExceptions;
    using BaseClass::getCmdSizeForHeaplessPrologue;
    using BaseClass::getCmdSizeForPrologue;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::getScratchSpaceController;
    using BaseClass::handleAllocationsResidencyForHeaplessProlog;
    using BaseClass::handleFrontEndStateTransition;
    using BaseClass::handleImmediateFlushStatelessAllocationsResidency;
    using BaseClass::handlePipelineSelectStateTransition;
    using BaseClass::handleStateBaseAddressStateTransition;
    using BaseClass::heapStorageRequiresRecyclingTag;
    using BaseClass::indirectHeap;
    using BaseClass::iohState;
    using BaseClass::isBlitterDirectSubmissionEnabled;
    using BaseClass::isDirectSubmissionEnabled;
    using BaseClass::isPerDssBackedBufferSent;
    using BaseClass::makeResident;
    using BaseClass::pageTableManagerInitialized;
    using BaseClass::perDssBackedBuffer;
    using BaseClass::pollForCompletion;
    using BaseClass::postInitFlagsSetup;
    using BaseClass::primaryCsr;
    using BaseClass::programActivePartitionConfig;
    using BaseClass::programEnginePrologue;
    using BaseClass::programExceptions;
    using BaseClass::programPerDssBackedBuffer;
    using BaseClass::programPreamble;
    using BaseClass::programStallingCommandsForBarrier;
    using BaseClass::programStallingNoPostSyncCommandsForBarrier;
    using BaseClass::programStallingPostSyncCommandsForBarrier;
    using BaseClass::programStateSip;
    using BaseClass::programVFEState;
    using BaseClass::requiresInstructionCacheFlush;
    using BaseClass::rootDeviceIndex;
    using BaseClass::sshState;
    using BaseClass::staticWorkPartitioningEnabled;
    using BaseClass::streamProperties;
    using BaseClass::wasSubmittedToSingleSubdevice;
    using BaseClass::CommandStreamReceiver::activePartitions;
    using BaseClass::CommandStreamReceiver::activePartitionsConfig;
    using BaseClass::CommandStreamReceiver::areExceptionsSent;
    using BaseClass::CommandStreamReceiver::baseWaitFunction;
    using BaseClass::CommandStreamReceiver::bindingTableBaseAddressRequired;
    using BaseClass::CommandStreamReceiver::checkForNewResources;
    using BaseClass::CommandStreamReceiver::checkImplicitFlushForGpuIdle;
    using BaseClass::CommandStreamReceiver::cleanupResources;
    using BaseClass::CommandStreamReceiver::clearColorAllocation;
    using BaseClass::CommandStreamReceiver::commandStream;
    using BaseClass::CommandStreamReceiver::debugConfirmationFunction;
    using BaseClass::CommandStreamReceiver::debugPauseStateAddress;
    using BaseClass::CommandStreamReceiver::debugSurface;
    using BaseClass::CommandStreamReceiver::deviceBitfield;
    using BaseClass::CommandStreamReceiver::dispatchMode;
    using BaseClass::CommandStreamReceiver::doubleSbaWa;
    using BaseClass::CommandStreamReceiver::downloadAllocationImpl;
    using BaseClass::CommandStreamReceiver::executionEnvironment;
    using BaseClass::CommandStreamReceiver::feSupportFlags;
    using BaseClass::CommandStreamReceiver::flushBcsTask;
    using BaseClass::CommandStreamReceiver::flushStamp;
    using BaseClass::CommandStreamReceiver::globalFenceAllocation;
    using BaseClass::CommandStreamReceiver::globalStatelessHeapAllocation;
    using BaseClass::CommandStreamReceiver::gpuHangCheckPeriod;
    using BaseClass::CommandStreamReceiver::gsbaFor32BitProgrammed;
    using BaseClass::CommandStreamReceiver::heaplessModeEnabled;
    using BaseClass::CommandStreamReceiver::heaplessStateInitEnabled;
    using BaseClass::CommandStreamReceiver::heaplessStateInitialized;
    using BaseClass::CommandStreamReceiver::immWritePostSyncWriteOffset;
    using BaseClass::CommandStreamReceiver::initDirectSubmission;
    using BaseClass::CommandStreamReceiver::internalAllocationStorage;
    using BaseClass::CommandStreamReceiver::isBlitterDirectSubmissionEnabled;
    using BaseClass::CommandStreamReceiver::isDirectSubmissionEnabled;
    using BaseClass::CommandStreamReceiver::isEnginePrologueSent;
    using BaseClass::CommandStreamReceiver::isPreambleSent;
    using BaseClass::CommandStreamReceiver::isStateSipSent;
    using BaseClass::CommandStreamReceiver::lastAdditionalKernelExecInfo;
    using BaseClass::CommandStreamReceiver::lastKernelExecutionType;
    using BaseClass::CommandStreamReceiver::lastMediaSamplerConfig;
    using BaseClass::CommandStreamReceiver::lastMemoryCompressionState;
    using BaseClass::CommandStreamReceiver::lastPreemptionMode;
    using BaseClass::CommandStreamReceiver::lastSentL3Config;
    using BaseClass::CommandStreamReceiver::lastSystolicPipelineSelectMode;
    using BaseClass::CommandStreamReceiver::lastVmeSubslicesConfig;
    using BaseClass::CommandStreamReceiver::latestFlushedTaskCount;
    using BaseClass::CommandStreamReceiver::latestSentStatelessMocsConfig;
    using BaseClass::CommandStreamReceiver::latestSentTaskCount;
    using BaseClass::CommandStreamReceiver::mediaVfeStateDirty;
    using BaseClass::CommandStreamReceiver::newResources;
    using BaseClass::CommandStreamReceiver::osContext;
    using BaseClass::CommandStreamReceiver::ownershipMutex;
    using BaseClass::CommandStreamReceiver::perfCounterAllocator;
    using BaseClass::CommandStreamReceiver::pipelineSupportFlags;
    using BaseClass::CommandStreamReceiver::preemptionAllocation;
    using BaseClass::CommandStreamReceiver::profilingTimeStampAllocator;
    using BaseClass::CommandStreamReceiver::requestedPreallocationsAmount;
    using BaseClass::CommandStreamReceiver::requiredScratchSlot0Size;
    using BaseClass::CommandStreamReceiver::requiredScratchSlot1Size;
    using BaseClass::CommandStreamReceiver::resourcesInitialized;
    using BaseClass::CommandStreamReceiver::samplerCacheFlushRequired;
    using BaseClass::CommandStreamReceiver::sbaSupportFlags;
    using BaseClass::CommandStreamReceiver::scratchSpaceController;
    using BaseClass::CommandStreamReceiver::stateComputeModeDirty;
    using BaseClass::CommandStreamReceiver::submissionAggregator;
    using BaseClass::CommandStreamReceiver::tagAddress;
    using BaseClass::CommandStreamReceiver::tagAllocation;
    using BaseClass::CommandStreamReceiver::taskCount;
    using BaseClass::CommandStreamReceiver::taskLevel;
    using BaseClass::CommandStreamReceiver::timestampPacketAllocator;
    using BaseClass::CommandStreamReceiver::timestampPacketWriteEnabled;
    using BaseClass::CommandStreamReceiver::timeStampPostSyncWriteOffset;
    using BaseClass::CommandStreamReceiver::use4GbHeaps;
    using BaseClass::CommandStreamReceiver::useGpuIdleImplicitFlush;
    using BaseClass::CommandStreamReceiver::useNewResourceImplicitFlush;
    using BaseClass::CommandStreamReceiver::useNotifyEnableForPostSync;
    using BaseClass::CommandStreamReceiver::userPauseConfirmation;
    using BaseClass::CommandStreamReceiver::waitForTaskCountAndCleanAllocationList;
    using BaseClass::CommandStreamReceiver::workPartitionAllocation;

    UltCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                             uint32_t rootDeviceIndex,
                             const DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield), recursiveLockCounter(0),
          recordedDispatchFlags(DispatchFlagsHelper::createDefaultDispatchFlags()),
          recordedBcsDispatchFlags(DispatchFlagsHelper::createDefaultBcsDispatchFlags()) {
        this->downloadAllocationImpl = [this](GraphicsAllocation &graphicsAllocation) {
            this->downloadAllocationUlt(graphicsAllocation);
        };
        gpuHangCheckPeriod = {};
    }
    ~UltCommandStreamReceiver() override {
        this->downloadAllocationImpl = nullptr;
    }
    static CommandStreamReceiver *create(bool withAubDump,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield) {
        return new UltCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    GmmPageTableMngr *createPageTableManager() override {
        createPageTableManagerCalled = true;
        return nullptr;
    }

    void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency, bool clearAllocations) override {
        makeSurfacePackNonResidentCalled++;
        BaseClass::makeSurfacePackNonResident(allocationsForResidency, clearAllocations);
    }

    NEO::SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        if (flushReturnValue) {
            return *flushReturnValue;
        }
        if (recordFlushedBatchBuffer) {
            latestFlushedBatchBuffer = batchBuffer;
        }
        latestSentTaskCountValueDuringFlush = latestSentTaskCount;
        return BaseClass::flush(batchBuffer, allocationsForResidency);
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        recordedDispatchFlags = dispatchFlags;
        recordedSsh = ssh;
        this->lastFlushedCommandStream = &commandStream;
        return BaseClass::flushTask(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    CompletionStamp flushImmediateTask(LinearStream &immediateCommandStream,
                                       size_t immediateCommandStreamStart,
                                       ImmediateDispatchFlags &dispatchFlags,
                                       Device &device) override {
        recordedImmediateDispatchFlags = dispatchFlags;
        this->lastFlushedCommandStream = &commandStream;
        this->lastFlushedImmediateCommandStream = &immediateCommandStream;
        return BaseClass::flushImmediateTask(immediateCommandStream, immediateCommandStreamStart, dispatchFlags, device);
    }

    CompletionStamp flushImmediateTaskStateless(LinearStream &immediateCommandStream,
                                                size_t immediateCommandStreamStart,
                                                ImmediateDispatchFlags &dispatchFlags,
                                                Device &device) override {
        recordedImmediateDispatchFlags = dispatchFlags;
        this->lastFlushedCommandStream = &commandStream;
        this->lastFlushedImmediateCommandStream = &immediateCommandStream;
        return BaseClass::flushImmediateTaskStateless(immediateCommandStream, immediateCommandStreamStart, dispatchFlags, device);
    }

    CompletionStamp flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                 const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) override {
        this->recordedBcsDispatchFlags = dispatchBcsFlags;
        this->lastFlushedBcsCommandStream = &commandStreamTask;
        return BaseClass::flushBcsTask(commandStreamTask, commandStreamTaskStart, dispatchBcsFlags, hwInfo);
    }

    SubmissionStatus initializeDeviceWithFirstSubmission(Device &device) override {
        initializeDeviceWithFirstSubmissionCalled++;
        return BaseClass::initializeDeviceWithFirstSubmission(device);
    }

    void programHeaplessStateProlog(Device &device, LinearStream &commandStream) override {
        this->commandStreamHeaplessStateInit = &commandStream;
        return BaseClass::programHeaplessStateProlog(device, commandStream);
    }

    bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        writeMemoryParams.totalCallCount++;
        if (isChunkCopy) {
            writeMemoryParams.chunkWriteCallCount++;
        }

        writeMemoryParams.latestGfxAllocation = &gfxAllocation;
        writeMemoryParams.latestChunkedMode = isChunkCopy;
        writeMemoryParams.latestGpuVaChunkOffset = gpuVaChunkOffset;
        writeMemoryParams.latestChunkSize = chunkSize;

        return BaseClass::writeMemory(gfxAllocation, isChunkCopy, gpuVaChunkOffset, chunkSize);
    }

    size_t getPreferredTagPoolSize() const override {
        return BaseClass::getPreferredTagPoolSize() + 1;
    }
    void setPreemptionAllocation(GraphicsAllocation *allocation) { this->preemptionAllocation = allocation; }

    void downloadAllocations(bool blockingWait, TaskCountType taskCount) override {
        downloadAllocationsCalledCount++;
        latestDownloadAllocationsBlocking = blockingWait;
    }

    void downloadAllocationUlt(GraphicsAllocation &gfxAllocation) {
        downloadAllocationCalled = true;
    }

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override {
        std::lock_guard<std::mutex> guard(mutex);
        latestWaitForCompletionWithTimeoutTaskCount.store(taskCountToWait);
        latestWaitForCompletionWithTimeoutWaitParams = params;
        waitForCompletionWithTimeoutTaskCountCalled++;
        if (callBaseWaitForCompletionWithTimeout) {
            return BaseClass::waitForCompletionWithTimeout(params, taskCountToWait);
        }
        return returnWaitForCompletionWithTimeout;
    }

    void fillReusableAllocationsList() override {
        fillReusableAllocationsListCalled++;
        if (callBaseFillReusableAllocationsList) {
            return BaseClass::fillReusableAllocationsList();
        }
    }

    WaitStatus waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, TaskCountType taskCountToWait) {
        return waitForCompletionWithTimeout(WaitParams{false, enableTimeout, false, timeoutMicroseconds}, taskCountToWait);
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override {
        if (captureWaitForTaskCountWithKmdNotifyInputParams) {
            static std::mutex waitForTaskCountWithKmdNotifyInputParamsMtx;
            std::unique_lock<std::mutex> lock(waitForTaskCountWithKmdNotifyInputParamsMtx);
            waitForTaskCountWithKmdNotifyInputParams.push_back({taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle});
        }
        if (waitForTaskCountWithKmdNotifyFallbackReturnValue.has_value()) {
            return *waitForTaskCountWithKmdNotifyFallbackReturnValue;
        }

        return BaseClass::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
    }

    WaitStatus waitForTaskCount(TaskCountType requiredTaskCount) override {
        this->waitForTaskCountCalled = true;
        if (waitForTaskCountReturnValue.has_value()) {
            return *waitForTaskCountReturnValue;
        }
        return BaseClass::waitForTaskCount(requiredTaskCount);
    }

    void overrideCsrSizeReqFlags(CsrSizeRequestFlags &flags) { this->csrSizeRequestFlags = flags; }
    GraphicsAllocation *getPreemptionAllocation() const { return this->preemptionAllocation; }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentCalledTimes++;
        if (storeMakeResidentAllocations) {
            std::map<GraphicsAllocation *, uint32_t>::iterator it = makeResidentAllocations.find(&gfxAllocation);
            if (it == makeResidentAllocations.end()) {
                std::pair<std::map<GraphicsAllocation *, uint32_t>::iterator, bool> result;
                result = makeResidentAllocations.insert(std::pair<GraphicsAllocation *, uint32_t>(&gfxAllocation, 1));
                DEBUG_BREAK_IF(!result.second);
            } else {
                makeResidentAllocations[&gfxAllocation]++;
            }
        }
        BaseClass::makeResident(gfxAllocation);
    }

    bool isMadeResident(GraphicsAllocation *graphicsAllocation) const {
        return makeResidentAllocations.find(graphicsAllocation) != makeResidentAllocations.end();
    }

    bool isMadeResident(GraphicsAllocation *graphicsAllocation, TaskCountType taskCount) const {
        auto it = makeResidentAllocations.find(graphicsAllocation);
        if (it == makeResidentAllocations.end()) {
            return false;
        }
        return (it->first->getTaskCount(osContext->getContextId()) == taskCount);
    }

    bool isMadeResident(GraphicsAllocation *graphicsAllocation, uint32_t residentCount) const {
        auto it = makeResidentAllocations.find(graphicsAllocation);
        if (it == makeResidentAllocations.end()) {
            return false;
        }
        return it->second == residentCount;
    }

    std::map<GraphicsAllocation *, uint32_t> makeResidentAllocations;
    bool storeMakeResidentAllocations = false;

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override {
        auto status = CommandStreamReceiverHw<GfxFamily>::checkAndActivateAubSubCapture(kernelName);
        checkAndActivateAubSubCaptureCalled = true;
        return status;
    }
    void addAubComment(const char *message) override {
        CommandStreamReceiverHw<GfxFamily>::addAubComment(message);
        aubCommentMessages.push_back(message);
        addAubCommentCalled = true;
    }
    bool flushBatchedSubmissions() override {
        auto commandStreamReceieverOwnership = this->obtainUniqueOwnership();
        flushBatchedSubmissionsCalled = true;

        if (shouldFailFlushBatchedSubmissions) {
            return false;
        }

        if (shouldFlushBatchedSubmissionsReturnSuccess) {
            return true;
        }

        return CommandStreamReceiverHw<GfxFamily>::flushBatchedSubmissions();
    }
    SubmissionStatus flushTagUpdate() override {
        flushTagUpdateCalled = true;
        auto ret = SubmissionStatus::success;
        if (this->callFlushTagUpdate) {
            ret = CommandStreamReceiverHw<GfxFamily>::flushTagUpdate();
        }
        return ret;
    }

    void initProgrammingFlags() override {
        CommandStreamReceiverHw<GfxFamily>::initProgrammingFlags();
        initProgrammingFlagsCalled = true;
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        recursiveLockCounter++;
        return CommandStreamReceiverHw<GfxFamily>::obtainUniqueOwnership();
    }

    TaskCountType flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, Device &device) override {
        blitBufferCalled++;
        receivedBlitProperties = blitPropertiesContainer;

        if (callBaseFlushBcsTask) {
            return CommandStreamReceiverHw<GfxFamily>::flushBcsTask(blitPropertiesContainer, blocking, device);
        } else {
            return flushBcsTaskReturnValue;
        }
    }

    bool createPerDssBackedBuffer(Device &device) override {
        createPerDssBackedBufferCalled++;
        return BaseClass::createPerDssBackedBuffer(device);
    }

    bool isMultiOsContextCapable() const override {
        if (callBaseIsMultiOsContextCapable) {
            return BaseClass::isMultiOsContextCapable();
        }
        return multiOsContextCapable;
    }

    bool initDirectSubmission() override {
        if (ultHwConfig.csrFailInitDirectSubmission) {
            return false;
        }
        initDirectSubmissionCalled++;
        return BaseClass::CommandStreamReceiver::initDirectSubmission();
    }

    bool isDirectSubmissionEnabled() const override {
        if (ultHwConfig.csrBaseCallDirectSubmissionAvailable) {
            return BaseClass::isDirectSubmissionEnabled();
        }
        if (ultHwConfig.csrSuperBaseCallDirectSubmissionAvailable) {
            return BaseClass::CommandStreamReceiver::isDirectSubmissionEnabled();
        }
        return directSubmissionAvailable;
    }

    bool isBlitterDirectSubmissionEnabled() const override {
        if (ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable) {
            return BaseClass::isBlitterDirectSubmissionEnabled();
        }
        if (ultHwConfig.csrSuperBaseCallBlitterDirectSubmissionAvailable) {
            return BaseClass::CommandStreamReceiver::isBlitterDirectSubmissionEnabled();
        }
        return blitterDirectSubmissionAvailable;
    }

    bool isKmdWaitOnTaskCountAllowed() const override {
        if (callBaseIsKmdWaitOnTaskCountAllowed) {
            return BaseClass::isKmdWaitOnTaskCountAllowed();
        }
        return isKmdWaitOnTaskCountAllowedValue;
    }

    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        createAllocationForHostSurfaceCalled++;
        cpuCopyForHostPtrSurfaceAllowed = surface.peekIsPtrCopyAllowed();
        return BaseClass::createAllocationForHostSurface(surface, requiresL3Flush);
    }

    void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize) override {
        ensureCommandBufferAllocationCalled++;
        BaseClass::ensureCommandBufferAllocation(commandStream, minimumRequiredSize, additionalAllocationSize);
    }

    CommandStreamReceiverType getType() const override {
        return commandStreamReceiverType;
    }

    void pollForCompletion(bool skipTaskCountCheck) override {
        pollForCompletionCalled++;
    }

    void pollForAubCompletion() override {
        pollForAubCompletionCalled++;
    }

    bool checkGpuHangDetected(CommandStreamReceiver::TimeType currentTime, CommandStreamReceiver::TimeType &lastHangCheckTime) const override {
        checkGpuHangDetectedCalled++;
        if (forceReturnGpuHang) {
            return true;
        }

        return BaseClass::checkGpuHangDetected(currentTime, lastHangCheckTime);
    }

    SubmissionStatus sendRenderStateCacheFlush() override {
        this->renderStateCacheFlushed = true;
        if (callBaseSendRenderStateCacheFlush) {
            return BaseClass::sendRenderStateCacheFlush();
        }
        return *flushReturnValue;
    }

    void stopDirectSubmission(bool blocking, bool needsLock) override {
        stopDirectSubmissionCalled = true;
        stopDirectSubmissionCalledBlocking = blocking;
        if (this->callBaseStopDirectSubmission) {
            BaseClass::stopDirectSubmission(blocking, needsLock);
        }
    }

    bool waitUserFence(TaskCountType waitValue, uint64_t hostAddress, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override {
        waitUserFenceParams.callCount++;
        waitUserFenceParams.latestWaitedAddress = hostAddress;
        waitUserFenceParams.latestWaitedValue = waitValue;
        waitUserFenceParams.latestWaitedTimeout = timeout;
        waitUserFenceParams.userInterrupt = timeout;
        waitUserFenceParams.externalInterruptId = externalInterruptId;
        waitUserFenceParams.latestAllocForInterruptWait = allocForInterruptWait;

        if (waitUserFenceParams.forceRetStatusEnabled) {
            return waitUserFenceParams.forceRetStatusValue;
        }

        return BaseClass::waitUserFence(waitValue, hostAddress, timeout, userInterrupt, externalInterruptId, allocForInterruptWait);
    }

    bool waitUserFenceSupported() override { return isUserFenceWaitSupported; }

    void unblockPagingFenceSemaphore(uint64_t pagingFenceValue) override {
        this->pagingFenceValueToUnblock = pagingFenceValue;
        BaseClass::unblockPagingFenceSemaphore(pagingFenceValue);
    }

    void drainPagingFenceQueue() override {
        drainPagingFenceQueueCalled++;
        BaseClass::drainPagingFenceQueue();
    }
    SubmissionStatus flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushHandlerCalled++;
        return BaseClass::flushHandler(batchBuffer, allocationsForResidency);
    }
    bool isAnyDirectSubmissionEnabled() const override {
        if (isAnyDirectSubmissionEnabledCallBase) {
            return BaseClass::isAnyDirectSubmissionEnabled();
        }
        return isAnyDirectSubmissionEnabledResult;
    }
    std::vector<std::string> aubCommentMessages;

    BatchBuffer latestFlushedBatchBuffer = {};

    std::atomic<TaskCountType> latestWaitForCompletionWithTimeoutTaskCount{0};
    TaskCountType latestSentTaskCountValueDuringFlush = 0;
    WaitParams latestWaitForCompletionWithTimeoutWaitParams{};
    WaitUserFenceParams waitUserFenceParams;
    WriteMemoryParams writeMemoryParams;
    TaskCountType flushBcsTaskReturnValue{};

    LinearStream *lastFlushedCommandStream = nullptr;
    LinearStream *lastFlushedImmediateCommandStream = nullptr;
    LinearStream *lastFlushedBcsCommandStream = nullptr;
    LinearStream *commandStreamHeaplessStateInit = nullptr;

    const IndirectHeap *recordedSsh = nullptr;

    struct WaitForTaskCountWithKmdNotifyParams {
        TaskCountType taskCountToWait;
        FlushStamp flushStampToWait;
        bool useQuickKmdSleep;
        QueueThrottle throttle;
    };

    std::vector<WaitForTaskCountWithKmdNotifyParams> waitForTaskCountWithKmdNotifyInputParams;

    std::mutex mutex;
    std::atomic<uint32_t> recursiveLockCounter;
    std::atomic<uint32_t> waitForCompletionWithTimeoutTaskCountCalled{0};
    std::atomic<uint64_t> pagingFenceValueToUnblock{0u};
    uint32_t makeResidentCalledTimes = 0;
    uint32_t makeSurfacePackNonResidentCalled = false;
    uint32_t blitBufferCalled = 0;
    uint32_t createPerDssBackedBufferCalled = 0;
    uint32_t initDirectSubmissionCalled = 0;
    uint32_t fillReusableAllocationsListCalled = 0;
    uint32_t pollForCompletionCalled = 0;
    uint32_t pollForAubCompletionCalled = 0;
    uint32_t initializeDeviceWithFirstSubmissionCalled = 0;
    uint32_t drainPagingFenceQueueCalled = 0;
    uint32_t flushHandlerCalled = 0;
    uint32_t obtainUniqueOwnershipCalledTimes = 0;
    mutable uint32_t checkGpuHangDetectedCalled = 0;
    int ensureCommandBufferAllocationCalled = 0;
    DispatchFlags recordedDispatchFlags;
    DispatchBcsFlags recordedBcsDispatchFlags;
    ImmediateDispatchFlags recordedImmediateDispatchFlags = {};
    BlitPropertiesContainer receivedBlitProperties = {};
    uint32_t createAllocationForHostSurfaceCalled = 0;
    WaitStatus returnWaitForCompletionWithTimeout = WaitStatus::ready;
    std::optional<WaitStatus> waitForTaskCountWithKmdNotifyFallbackReturnValue{};
    std::optional<WaitStatus> waitForTaskCountReturnValue{};
    std::optional<SubmissionStatus> flushReturnValue{};
    CommandStreamReceiverType commandStreamReceiverType = CommandStreamReceiverType::hardware;
    std::atomic<uint32_t> downloadAllocationsCalledCount = 0;
    std::atomic<bool> latestDownloadAllocationsBlocking = false;

    bool renderStateCacheFlushed = false;
    bool cpuCopyForHostPtrSurfaceAllowed = false;
    bool createPageTableManagerCalled = false;
    bool recordFlushedBatchBuffer = false;
    bool checkAndActivateAubSubCaptureCalled = false;
    bool addAubCommentCalled = false;
    bool waitForTaskCountCalled = false;
    std::atomic_bool downloadAllocationCalled = false;
    std::atomic_bool downloadAllocationsCalled = false;
    bool flushBatchedSubmissionsCalled = false;
    bool flushTagUpdateCalled = false;
    bool callFlushTagUpdate = true;
    bool callBaseStopDirectSubmission = true;
    bool initProgrammingFlagsCalled = false;
    bool multiOsContextCapable = false;
    bool memoryCompressionEnabled = false;
    bool directSubmissionAvailable = false;
    bool blitterDirectSubmissionAvailable = false;
    bool callBaseIsMultiOsContextCapable = false;
    bool callBaseWaitForCompletionWithTimeout = true;
    bool shouldFailFlushBatchedSubmissions = false;
    bool shouldFlushBatchedSubmissionsReturnSuccess = false;
    bool callBaseFillReusableAllocationsList = false;
    bool callBaseFlushBcsTask{true};
    bool callBaseSendRenderStateCacheFlush = true;
    bool forceReturnGpuHang = false;
    bool callBaseIsKmdWaitOnTaskCountAllowed = false;
    bool isKmdWaitOnTaskCountAllowedValue = false;
    bool stopDirectSubmissionCalled = false;
    bool stopDirectSubmissionCalledBlocking = false;
    bool isUserFenceWaitSupported = false;
    bool isAnyDirectSubmissionEnabledCallBase = true;
    bool isAnyDirectSubmissionEnabledResult = true;
    std::atomic_bool captureWaitForTaskCountWithKmdNotifyInputParams = false;
};

} // namespace NEO

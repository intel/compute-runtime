/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_experimental_command_buffer.h"

#include <map>
#include <optional>

namespace NEO {

class GmmPageTableMngr;

template <typename GfxFamily>
class UltCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily>, public NonCopyableOrMovableClass {
    using BaseClass = CommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::addPipeControlBefore3dState;
    using BaseClass::blitterDirectSubmission;
    using BaseClass::checkPlatformSupportsGpuIdleImplicitFlush;
    using BaseClass::checkPlatformSupportsNewResourceImplicitFlush;
    using BaseClass::createKernelArgsBufferAllocation;
    using BaseClass::csrSizeRequestFlags;
    using BaseClass::directSubmission;
    using BaseClass::dshState;
    using BaseClass::getCmdSizeForPrologue;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::getScratchSpaceController;
    using BaseClass::handleFrontEndStateTransition;
    using BaseClass::handlePipelineSelectStateTransition;
    using BaseClass::indirectHeap;
    using BaseClass::iohState;
    using BaseClass::isBlitterDirectSubmissionEnabled;
    using BaseClass::isDirectSubmissionEnabled;
    using BaseClass::isPerDssBackedBufferSent;
    using BaseClass::kernelArgsBufferAllocation;
    using BaseClass::logicalStateHelper;
    using BaseClass::makeResident;
    using BaseClass::perDssBackedBuffer;
    using BaseClass::postInitFlagsSetup;
    using BaseClass::programActivePartitionConfig;
    using BaseClass::programEnginePrologue;
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
    using BaseClass::CommandStreamReceiver::baseWaitFunction;
    using BaseClass::CommandStreamReceiver::bindingTableBaseAddressRequired;
    using BaseClass::CommandStreamReceiver::canUse4GbHeaps;
    using BaseClass::CommandStreamReceiver::checkForNewResources;
    using BaseClass::CommandStreamReceiver::checkImplicitFlushForGpuIdle;
    using BaseClass::CommandStreamReceiver::cleanupResources;
    using BaseClass::CommandStreamReceiver::clearColorAllocation;
    using BaseClass::CommandStreamReceiver::commandStream;
    using BaseClass::CommandStreamReceiver::debugConfirmationFunction;
    using BaseClass::CommandStreamReceiver::debugPauseStateAddress;
    using BaseClass::CommandStreamReceiver::deviceBitfield;
    using BaseClass::CommandStreamReceiver::dispatchMode;
    using BaseClass::CommandStreamReceiver::downloadAllocationImpl;
    using BaseClass::CommandStreamReceiver::executionEnvironment;
    using BaseClass::CommandStreamReceiver::experimentalCmdBuffer;
    using BaseClass::CommandStreamReceiver::feSupportFlags;
    using BaseClass::CommandStreamReceiver::flushStamp;
    using BaseClass::CommandStreamReceiver::globalFenceAllocation;
    using BaseClass::CommandStreamReceiver::gpuHangCheckPeriod;
    using BaseClass::CommandStreamReceiver::GSBAFor32BitProgrammed;
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
    using BaseClass::CommandStreamReceiver::lastSentUseGlobalAtomics;
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
    using BaseClass::CommandStreamReceiver::postSyncWriteOffset;
    using BaseClass::CommandStreamReceiver::profilingTimeStampAllocator;
    using BaseClass::CommandStreamReceiver::requiredPrivateScratchSize;
    using BaseClass::CommandStreamReceiver::requiredScratchSize;
    using BaseClass::CommandStreamReceiver::samplerCacheFlushRequired;
    using BaseClass::CommandStreamReceiver::scratchSpaceController;
    using BaseClass::CommandStreamReceiver::stallingCommandsOnNextFlushRequired;
    using BaseClass::CommandStreamReceiver::submissionAggregator;
    using BaseClass::CommandStreamReceiver::tagAddress;
    using BaseClass::CommandStreamReceiver::taskCount;
    using BaseClass::CommandStreamReceiver::taskLevel;
    using BaseClass::CommandStreamReceiver::timestampPacketAllocator;
    using BaseClass::CommandStreamReceiver::timestampPacketWriteEnabled;
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
          recordedDispatchFlags(DispatchFlagsHelper::createDefaultDispatchFlags()) {
        this->downloadAllocationImpl = [this](GraphicsAllocation &graphicsAllocation) {
            this->downloadAllocationUlt(graphicsAllocation);
        };
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
        if (recordFlusheBatchBuffer) {
            latestFlushedBatchBuffer = batchBuffer;
        }
        latestSentTaskCountValueDuringFlush = latestSentTaskCount;
        return BaseClass::flush(batchBuffer, allocationsForResidency);
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        recordedDispatchFlags = dispatchFlags;
        this->lastFlushedCommandStream = &commandStream;
        return BaseClass::flushTask(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    size_t getPreferredTagPoolSize() const override {
        return BaseClass::getPreferredTagPoolSize() + 1;
    }
    void setPreemptionAllocation(GraphicsAllocation *allocation) { this->preemptionAllocation = allocation; }

    void downloadAllocations() override {
        downloadAllocationCalled = true;
        downloadAllocationsCalled = true;
    }

    void downloadAllocationUlt(GraphicsAllocation &gfxAllocation) {
        downloadAllocationCalled = true;
    }

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        latestWaitForCompletionWithTimeoutTaskCount.store(taskCountToWait);
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

    WaitStatus waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) {
        return waitForCompletionWithTimeout(WaitParams{false, enableTimeout, timeoutMicroseconds}, taskCountToWait);
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override {
        if (waitForTaskCountWithKmdNotifyFallbackReturnValue.has_value()) {
            return *waitForTaskCountWithKmdNotifyFallbackReturnValue;
        }

        return BaseClass::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
    }

    void overrideCsrSizeReqFlags(CsrSizeRequestFlags &flags) { this->csrSizeRequestFlags = flags; }
    GraphicsAllocation *getPreemptionAllocation() const { return this->preemptionAllocation; }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
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

    bool isMadeResident(GraphicsAllocation *graphicsAllocation, uint32_t taskCount) const {
        auto it = makeResidentAllocations.find(graphicsAllocation);
        if (it == makeResidentAllocations.end()) {
            return false;
        }
        return (it->first->getTaskCount(osContext->getContextId()) == taskCount);
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
        flushBatchedSubmissionsCalled = true;

        if (shouldFailFlushBatchedSubmissions) {
            return false;
        }

        if (shouldFlushBatchedSubmissionsReturnSuccess) {
            return true;
        }

        return CommandStreamReceiverHw<GfxFamily>::flushBatchedSubmissions();
    }
    void initProgrammingFlags() override {
        CommandStreamReceiverHw<GfxFamily>::initProgrammingFlags();
        initProgrammingFlagsCalled = true;
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        recursiveLockCounter++;
        return CommandStreamReceiverHw<GfxFamily>::obtainUniqueOwnership();
    }

    std::optional<uint32_t> flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override {
        blitBufferCalled++;
        receivedBlitProperties = blitPropertiesContainer;

        if (callBaseFlushBcsTask) {
            return CommandStreamReceiverHw<GfxFamily>::flushBcsTask(blitPropertiesContainer, blocking, profilingEnabled, device);
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

    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        createAllocationForHostSurfaceCalled++;
        cpuCopyForHostPtrSurfaceAllowed = surface.peekIsPtrCopyAllowed();
        auto status = BaseClass::createAllocationForHostSurface(surface, requiresL3Flush);
        if (status)
            surface.getAllocation()->hostPtrTaskCountAssignment--;
        return status;
    }

    void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize) override {
        ensureCommandBufferAllocationCalled++;
        BaseClass::ensureCommandBufferAllocation(commandStream, minimumRequiredSize, additionalAllocationSize);
    }

    std::vector<std::string> aubCommentMessages;

    BatchBuffer latestFlushedBatchBuffer = {};

    std::atomic<uint32_t> recursiveLockCounter;
    std::atomic<uint32_t> latestWaitForCompletionWithTimeoutTaskCount{0};
    std::atomic<uint32_t> waitForCompletionWithTimeoutTaskCountCalled{0};

    LinearStream *lastFlushedCommandStream = nullptr;

    uint32_t makeSurfacePackNonResidentCalled = false;
    uint32_t latestSentTaskCountValueDuringFlush = 0;
    uint32_t blitBufferCalled = 0;
    uint32_t createPerDssBackedBufferCalled = 0;
    uint32_t initDirectSubmissionCalled = 0;
    uint32_t fillReusableAllocationsListCalled = 0;
    int ensureCommandBufferAllocationCalled = 0;
    DispatchFlags recordedDispatchFlags;
    BlitPropertiesContainer receivedBlitProperties = {};
    uint32_t createAllocationForHostSurfaceCalled = 0;
    bool cpuCopyForHostPtrSurfaceAllowed = false;

    bool createPageTableManagerCalled = false;
    bool recordFlusheBatchBuffer = false;
    bool checkAndActivateAubSubCaptureCalled = false;
    bool addAubCommentCalled = false;
    std::atomic_bool downloadAllocationCalled = false;
    std::atomic_bool downloadAllocationsCalled = false;
    bool flushBatchedSubmissionsCalled = false;
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
    WaitStatus returnWaitForCompletionWithTimeout = WaitStatus::Ready;
    std::optional<WaitStatus> waitForTaskCountWithKmdNotifyFallbackReturnValue{};
    bool callBaseFlushBcsTask{true};
    std::optional<uint32_t> flushBcsTaskReturnValue{};
};

} // namespace NEO

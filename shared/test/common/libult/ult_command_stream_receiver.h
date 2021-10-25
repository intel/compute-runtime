/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_experimental_command_buffer.h"

#include <map>
#include <memory>

namespace NEO {

class GmmPageTableMngr;

template <typename GfxFamily>
class UltCommandStreamReceiver : public CommandStreamReceiverHw<GfxFamily>, public NonCopyableOrMovableClass {
    using BaseClass = CommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::blitterDirectSubmission;
    using BaseClass::checkPlatformSupportsGpuIdleImplicitFlush;
    using BaseClass::checkPlatformSupportsNewResourceImplicitFlush;
    using BaseClass::directSubmission;
    using BaseClass::dshState;
    using BaseClass::getCmdSizeForPrologue;
    using BaseClass::getScratchPatchAddress;
    using BaseClass::getScratchSpaceController;
    using BaseClass::indirectHeap;
    using BaseClass::iohState;
    using BaseClass::isBlitterDirectSubmissionEnabled;
    using BaseClass::isDirectSubmissionEnabled;
    using BaseClass::isPerDssBackedBufferSent;
    using BaseClass::makeResident;
    using BaseClass::perDssBackedBuffer;
    using BaseClass::postInitFlagsSetup;
    using BaseClass::programActivePartitionConfig;
    using BaseClass::programEnginePrologue;
    using BaseClass::programPerDssBackedBuffer;
    using BaseClass::programPreamble;
    using BaseClass::programStateSip;
    using BaseClass::programVFEState;
    using BaseClass::requiresInstructionCacheFlush;
    using BaseClass::rootDeviceIndex;
    using BaseClass::sshState;
    using BaseClass::staticWorkPartitioningEnabled;
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
    using BaseClass::CommandStreamReceiver::executionEnvironment;
    using BaseClass::CommandStreamReceiver::experimentalCmdBuffer;
    using BaseClass::CommandStreamReceiver::flushStamp;
    using BaseClass::CommandStreamReceiver::globalFenceAllocation;
    using BaseClass::CommandStreamReceiver::GSBAFor32BitProgrammed;
    using BaseClass::CommandStreamReceiver::initDirectSubmission;
    using BaseClass::CommandStreamReceiver::internalAllocationStorage;
    using BaseClass::CommandStreamReceiver::isBlitterDirectSubmissionEnabled;
    using BaseClass::CommandStreamReceiver::isDirectSubmissionEnabled;
    using BaseClass::CommandStreamReceiver::isEnginePrologueSent;
    using BaseClass::CommandStreamReceiver::isPreambleSent;
    using BaseClass::CommandStreamReceiver::isStateSipSent;
    using BaseClass::CommandStreamReceiver::lastKernelExecutionType;
    using BaseClass::CommandStreamReceiver::lastMediaSamplerConfig;
    using BaseClass::CommandStreamReceiver::lastMemoryCompressionState;
    using BaseClass::CommandStreamReceiver::lastPreemptionMode;
    using BaseClass::CommandStreamReceiver::lastSentCoherencyRequest;
    using BaseClass::CommandStreamReceiver::lastSentL3Config;
    using BaseClass::CommandStreamReceiver::lastSentThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::lastSentUseGlobalAtomics;
    using BaseClass::CommandStreamReceiver::lastSpecialPipelineSelectMode;
    using BaseClass::CommandStreamReceiver::lastVmeSubslicesConfig;
    using BaseClass::CommandStreamReceiver::latestFlushedTaskCount;
    using BaseClass::CommandStreamReceiver::latestSentStatelessMocsConfig;
    using BaseClass::CommandStreamReceiver::latestSentTaskCount;
    using BaseClass::CommandStreamReceiver::mediaVfeStateDirty;
    using BaseClass::CommandStreamReceiver::newResources;
    using BaseClass::CommandStreamReceiver::osContext;
    using BaseClass::CommandStreamReceiver::perfCounterAllocator;
    using BaseClass::CommandStreamReceiver::profilingTimeStampAllocator;
    using BaseClass::CommandStreamReceiver::requiredPrivateScratchSize;
    using BaseClass::CommandStreamReceiver::requiredScratchSize;
    using BaseClass::CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using BaseClass::CommandStreamReceiver::samplerCacheFlushRequired;
    using BaseClass::CommandStreamReceiver::scratchSpaceController;
    using BaseClass::CommandStreamReceiver::stallingPipeControlOnNextFlushRequired;
    using BaseClass::CommandStreamReceiver::submissionAggregator;
    using BaseClass::CommandStreamReceiver::tagAddress;
    using BaseClass::CommandStreamReceiver::taskCount;
    using BaseClass::CommandStreamReceiver::taskLevel;
    using BaseClass::CommandStreamReceiver::timestampPacketAllocator;
    using BaseClass::CommandStreamReceiver::timestampPacketWriteEnabled;
    using BaseClass::CommandStreamReceiver::useGpuIdleImplicitFlush;
    using BaseClass::CommandStreamReceiver::useNewResourceImplicitFlush;
    using BaseClass::CommandStreamReceiver::userPauseConfirmation;
    using BaseClass::CommandStreamReceiver::waitForTaskCountAndCleanAllocationList;

    UltCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                             uint32_t rootDeviceIndex,
                             const DeviceBitfield deviceBitfield)
        : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield), recursiveLockCounter(0),
          recordedDispatchFlags(DispatchFlagsHelper::createDefaultDispatchFlags()) {}
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

    void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency) override {
        makeSurfacePackNonResidentCalled++;
        BaseClass::makeSurfacePackNonResident(allocationsForResidency);
    }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        if (recordFlusheBatchBuffer) {
            latestFlushedBatchBuffer = batchBuffer;
        }
        latestSentTaskCountValueDuringFlush = latestSentTaskCount;
        return BaseClass::flush(batchBuffer, allocationsForResidency);
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
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
    }

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) override {
        latestWaitForCompletionWithTimeoutTaskCount.store(taskCountToWait);
        waitForCompletionWithTimeoutTaskCountCalled++;
        if (callBaseWaitForCompletionWithTimeout) {
            return BaseClass::waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, taskCountToWait);
        }
        return returnWaitForCompletionWithTimeout;
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

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override {
        blitBufferCalled++;
        receivedBlitProperties = blitPropertiesContainer;
        return CommandStreamReceiverHw<GfxFamily>::blitBuffer(blitPropertiesContainer, blocking, profilingEnabled, device);
    }

    bool createPerDssBackedBuffer(Device &device) override {
        createPerDssBackedBufferCalled++;
        bool result = BaseClass::createPerDssBackedBuffer(device);
        if (!perDssBackedBuffer) {
            AllocationProperties properties{device.getRootDeviceIndex(), MemoryConstants::pageSize, GraphicsAllocation::AllocationType::INTERNAL_HEAP, device.getDeviceBitfield()};
            perDssBackedBuffer = executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(properties);
        }
        return result;
    }

    bool isMultiOsContextCapable() const override {
        if (callBaseIsMultiOsContextCapable) {
            return BaseClass::isMultiOsContextCapable();
        }
        return multiOsContextCapable;
    }

    bool initDirectSubmission(Device &device, OsContext &osContext) override {
        if (ultHwConfig.csrFailInitDirectSubmission) {
            return false;
        }
        initDirectSubmissionCalled++;
        return BaseClass::CommandStreamReceiver::initDirectSubmission(device, osContext);
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
    int ensureCommandBufferAllocationCalled = 0;
    DispatchFlags recordedDispatchFlags;
    BlitPropertiesContainer receivedBlitProperties = {};

    bool createPageTableManagerCalled = false;
    bool recordFlusheBatchBuffer = false;
    bool checkAndActivateAubSubCaptureCalled = false;
    bool addAubCommentCalled = false;
    bool downloadAllocationCalled = false;
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;
    bool multiOsContextCapable = false;
    bool memoryCompressionEnabled = false;
    bool directSubmissionAvailable = false;
    bool blitterDirectSubmissionAvailable = false;
    bool callBaseIsMultiOsContextCapable = false;
    bool callBaseWaitForCompletionWithTimeout = true;
    bool returnWaitForCompletionWithTimeout = true;
};
} // namespace NEO

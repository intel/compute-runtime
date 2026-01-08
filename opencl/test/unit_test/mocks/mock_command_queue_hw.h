/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/libult/ult_command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/dispatch_info.h"

namespace NEO {

template <typename GfxFamily>
class MockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    using BaseClass = CommandQueueHw<GfxFamily>;

  public:
    using BaseClass::bcsAllowed;
    using BaseClass::bcsEngineCount;
    using BaseClass::bcsEngines;
    using BaseClass::bcsInitialized;
    using BaseClass::bcsQueueEngineType;
    using BaseClass::bcsSplitInitialized;
    using BaseClass::bcsStates;
    using BaseClass::bcsTimestampPacketContainers;
    using BaseClass::blitEnqueueAllowed;
    using BaseClass::commandQueueProperties;
    using BaseClass::commandStream;
    using BaseClass::deferredTimestampPackets;
    using BaseClass::getDevice;
    using BaseClass::gpgpuEngine;
    using BaseClass::heaplessModeEnabled;
    using BaseClass::heaplessStateInitEnabled;
    using BaseClass::isBlitAuxTranslationRequired;
    using BaseClass::isCacheFlushOnNextBcsWriteRequired;
    using BaseClass::isCompleted;
    using BaseClass::isForceStateless;
    using BaseClass::isGpgpuSubmissionForBcsRequired;
    using BaseClass::isWalkerPostSyncSkipEnabled;
    using BaseClass::l3FlushAfterPostSyncEnabled;
    using BaseClass::latestSentEnqueueType;
    using BaseClass::minimalSizeForBcsSplit;
    using BaseClass::obtainCommandStream;
    using BaseClass::obtainNewTimestampPacketNodes;
    using BaseClass::overrideEngine;
    using BaseClass::prepareCsrDependency;
    using BaseClass::processDispatchForKernels;
    using BaseClass::relaxedOrderingForGpgpuAllowed;
    using BaseClass::splitBarrierRequired;
    using BaseClass::taskCount;
    using BaseClass::throttle;
    using BaseClass::timestampPacketContainer;

    MockCommandQueueHw(Context *context,
                       ClDevice *device,
                       cl_queue_properties *properties) : MockCommandQueueHw(context, device, properties, false) {}

    MockCommandQueueHw(Context *context,
                       ClDevice *device,
                       cl_queue_properties *properties, bool isInternal) : BaseClass(context, device, properties, isInternal) {
        this->constructBcsEngine(false);
    }

    void clearBcsStates() {
        CopyEngineState unusedState{};
        std::fill(bcsStates.begin(), bcsStates.end(), unusedState);
    }

    void clearBcsEngines() {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
    }

    void insertBcsEngine(aub_stream::EngineType bcsEngineType) {
        const auto index = NEO::EngineHelpers::getBcsIndex(bcsEngineType);
        const auto engine = &getDevice().getEngine(bcsEngineType, EngineUsage::regular);
        bcsEngines[index] = engine;
        bcsQueueEngineType = bcsEngineType;
    }

    cl_int flush() override {
        flushCalled = true;
        return BaseClass::flush();
    }

    void setOoqEnabled() {
        commandQueueProperties |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    }

    void setProfilingEnabled() {
        commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    }

    LinearStream &getCS(size_t minRequiredSize) override {
        requestedCmdStreamSize = minRequiredSize;
        return CommandQueue::getCS(minRequiredSize);
    }

    UltCommandStreamReceiver<GfxFamily> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<GfxFamily> &>(BaseClass::getGpgpuCommandStreamReceiver());
    }

    cl_int enqueueWriteImageImpl(Image *dstImage,
                                 cl_bool blockingWrite,
                                 const size_t *origin,
                                 const size_t *region,
                                 size_t inputRowPitch,
                                 size_t inputSlicePitch,
                                 const void *ptr,
                                 GraphicsAllocation *mapAllocation,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event,
                                 CommandStreamReceiver &csr) override {
        enqueueWriteImageCounter++;
        if (enqueueWriteImageCallBase) {
            return BaseClass::enqueueWriteImageImpl(dstImage,
                                                    blockingWrite,
                                                    origin,
                                                    region,
                                                    inputRowPitch,
                                                    inputSlicePitch,
                                                    ptr,
                                                    mapAllocation,
                                                    numEventsInWaitList,
                                                    eventWaitList,
                                                    event,
                                                    csr);
        }
        return CL_INVALID_OPERATION;
    }
    cl_int enqueueReadImageImpl(Image *srcImage,
                                cl_bool blockingRead,
                                const size_t *origin,
                                const size_t *region,
                                size_t rowPitch,
                                size_t slicePitch,
                                void *ptr,
                                GraphicsAllocation *mapAllocation,
                                cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList,
                                cl_event *event, CommandStreamReceiver &csr) override {
        enqueueReadImageCounter++;
        if (enqueueReadImageCallBase) {
            return BaseClass::enqueueReadImageImpl(srcImage,
                                                   blockingRead,
                                                   origin,
                                                   region,
                                                   rowPitch,
                                                   slicePitch,
                                                   ptr,
                                                   mapAllocation,
                                                   numEventsInWaitList,
                                                   eventWaitList,
                                                   event,
                                                   csr);
        }
        return CL_INVALID_OPERATION;
    }
    void *cpuDataTransferHandler(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &retVal) override {
        cpuDataTransferHandlerCalled = true;
        return BaseClass::cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    }
    cl_int enqueueWriteBufferImpl(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t size, const void *ptr, GraphicsAllocation *mapAllocation,
                                  cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override {
        enqueueWriteBufferCounter++;
        blockingWriteBuffer = blockingWrite == CL_TRUE;
        if (enqueueWriteBufferCallBase) {
            return BaseClass::enqueueWriteBufferImpl(buffer, blockingWrite, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event, csr);
        }
        return CL_INVALID_OPERATION;
    }

    cl_int enqueueReadBufferImpl(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t size, void *ptr, GraphicsAllocation *mapAllocation,
                                 cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override {
        enqueueReadBufferCounter++;
        blockingReadBuffer = blockingRead == CL_TRUE;
        if (enqueueReadBufferCallBase) {
            return BaseClass::enqueueReadBufferImpl(buffer, blockingRead, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event, csr);
        }
        return CL_INVALID_OPERATION;
    }

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        kernelParams = dispatchInfo.peekBuiltinOpParams();
        lastCommandType = commandType;

        for (auto &di : dispatchInfo) {
            lastEnqueuedKernels.push_back(di.getKernel());
            if (storeMultiDispatchInfo) {
                storedMultiDispatchInfo.push(di);
            }
        }
    }

    void notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead, bool notifyBcsCsr) override {
        notifyEnqueueReadBufferCalled = true;
        useBcsCsrOnNotifyEnabled = notifyBcsCsr;
    }
    void notifyEnqueueReadImage(Image *image, bool blockingRead, bool notifyBcsCsr) override {
        notifyEnqueueReadImageCalled = true;
        useBcsCsrOnNotifyEnabled = notifyBcsCsr;
    }
    void notifyEnqueueSVMMemcpy(GraphicsAllocation *gfxAllocation, bool blockingCopy, bool notifyBcsCsr) override {
        notifyEnqueueSVMMemcpyCalled = true;
        useBcsCsrOnNotifyEnabled = notifyBcsCsr;
    }

    WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
        this->recordedSkipWait = skipWait;
        latestTaskCountWaited = gpgpuTaskCountToWait;
        if (waitUntilCompleteReturnValue.has_value()) {
            return *waitUntilCompleteReturnValue;
        }

        return BaseClass::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
    }

    WaitStatus waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList, bool waitForTaskCountRequired) override {
        waitForAllEnginesCalledCount++;

        if (waitForAllEnginesReturnValue.has_value()) {
            return *waitForAllEnginesReturnValue;
        }

        return BaseClass::waitForAllEngines(blockedQueue, printfHandler, cleanTemporaryAllocationsList, waitForTaskCountRequired);
    }

    bool isCacheFlushForBcsRequired() const override {
        if (overrideIsCacheFlushForBcsRequired.enabled) {
            return overrideIsCacheFlushForBcsRequired.returnValue;
        }
        return BaseClass::isCacheFlushForBcsRequired();
    }

    bool blitEnqueueImageAllowed(const size_t *origin, const size_t *region, const Image &image) const override {
        isBlitEnqueueImageAllowed = BaseClass::blitEnqueueImageAllowed(origin, region, image);
        return isBlitEnqueueImageAllowed;
    }
    bool isQueueBlocked() override {
        if (setQueueBlocked != -1) {
            return setQueueBlocked;
        }
        return BaseClass::isQueueBlocked();
    }
    bool isGpgpuSubmissionForBcsRequired(bool queueBlocked, TimestampPacketDependencies &timestampPacketDependencies, bool containsCrossEngineDependency, bool textureCacheFlushRequired) const override {
        if (forceGpgpuSubmissionForBcsRequired != -1) {
            return forceGpgpuSubmissionForBcsRequired;
        }
        return BaseClass::isGpgpuSubmissionForBcsRequired(queueBlocked, timestampPacketDependencies, containsCrossEngineDependency, textureCacheFlushRequired);
    }

    bool waitForTimestamps(std::span<CopyEngineState> copyEnginesToWait, WaitStatus &status, TimestampPacketContainer *mainContainer, TimestampPacketContainer *deferredContainer) override {
        waitForTimestampsCalled = true;

        latestWaitForTimestampsStatus = BaseClass::waitForTimestamps(copyEnginesToWait, status, mainContainer, deferredContainer);

        return latestWaitForTimestampsStatus;
    }

    bool isCompleted(TaskCountType gpgpuTaskCount, std::span<const CopyEngineState> bcsStates) override {
        isCompletedCalled++;

        return CommandQueue::isCompleted(gpgpuTaskCount, bcsStates);
    }

    cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
        enqueueMarkerWithWaitListCalledCount++;
        return BaseClass::enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);
    }

    cl_int enqueueSVMMemcpy(cl_bool blockingCopy, void *dstPtr, const void *srcPtr, size_t size,
                            cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver *csrParam) override {
        enqueueSVMMemcpyCalledCount++;
        return BaseClass::enqueueSVMMemcpy(blockingCopy, dstPtr, srcPtr, size, numEventsInWaitList, eventWaitList, event, csrParam);
    }

    cl_int finish(bool resolvePendingL3Flushes) override {
        finishCalledCount++;
        return BaseClass::finish(resolvePendingL3Flushes);
    }

    LinearStream *peekCommandStream() {
        return this->commandStream;
    }

    ADDMETHOD(processDispatchForBlitEnqueue, BlitProperties, true, {}, (CommandStreamReceiver & blitCommandStreamReceiver, const MultiDispatchInfo &multiDispatchInfo, TimestampPacketDependencies &timestampPacketDependencies, const EventsRequest &eventsRequest, LinearStream *commandStream, uint32_t commandType, bool queueBlocked, bool profilingEnabled, TagNodeBase *multiRootDeviceEventSync), (blitCommandStreamReceiver, multiDispatchInfo, timestampPacketDependencies, eventsRequest, commandStream, commandType, queueBlocked, profilingEnabled, multiRootDeviceEventSync));

    ADDMETHOD(enqueueCommandWithoutKernel, CompletionStamp, true, {}, (Surface * *surfaces, size_t surfaceCount, LinearStream *commandStream, size_t commandStreamStart, bool &blocking, const EnqueueProperties &enqueueProperties, TimestampPacketDependencies &timestampPacketDependencies, EventsRequest &eventsRequest, EventBuilder &eventBuilder, TaskCountType taskLevel, CsrDependencies &csrDeps, CommandStreamReceiver *bcsCsr, bool hasRelaxedOrderingDependencies), (surfaces, surfaceCount, commandStream, commandStreamStart, blocking, enqueueProperties, timestampPacketDependencies, eventsRequest, eventBuilder, taskLevel, csrDeps, bcsCsr, hasRelaxedOrderingDependencies));

    std::vector<Kernel *> lastEnqueuedKernels;
    MultiDispatchInfo storedMultiDispatchInfo;
    BuiltinOpParams kernelParams;
    size_t enqueueWriteImageCounter = 0;
    size_t enqueueReadImageCounter = 0;
    size_t enqueueWriteBufferCounter = 0;
    size_t enqueueReadBufferCounter = 0;
    size_t requestedCmdStreamSize = 0;
    size_t enqueueSVMMemcpyCalledCount = 0;
    size_t finishCalledCount = 0;
    std::atomic<TaskCountType> latestTaskCountWaited{std::numeric_limits<uint32_t>::max()};
    std::atomic<uint32_t> isCompletedCalled = 0;
    unsigned int lastCommandType;
    int setQueueBlocked = -1;
    int forceGpgpuSubmissionForBcsRequired = -1;
    int waitForAllEnginesCalledCount = 0;
    int enqueueMarkerWithWaitListCalledCount = 0;
    std::optional<WaitStatus> waitForAllEnginesReturnValue{};
    std::optional<WaitStatus> waitUntilCompleteReturnValue{};
    struct OverrideReturnValue {
        bool enabled = false;
        bool returnValue = false;
    } overrideIsCacheFlushForBcsRequired;
    bool enqueueWriteImageCallBase = true;
    bool enqueueReadImageCallBase = true;
    bool enqueueWriteBufferCallBase = true;
    bool enqueueReadBufferCallBase = true;
    bool blockingWriteBuffer = false;
    bool blockingReadBuffer = false;
    bool storeMultiDispatchInfo = false;
    bool notifyEnqueueReadBufferCalled = false;
    bool notifyEnqueueReadImageCalled = false;
    bool notifyEnqueueSVMMemcpyCalled = false;
    bool cpuDataTransferHandlerCalled = false;
    bool useBcsCsrOnNotifyEnabled = false;
    bool waitForTimestampsCalled = false;
    bool latestWaitForTimestampsStatus = false;
    bool recordedSkipWait = false;
    bool flushCalled = false;
    mutable bool isBlitEnqueueImageAllowed = false;
};
} // namespace NEO

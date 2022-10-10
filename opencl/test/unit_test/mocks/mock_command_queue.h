/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

#include "opencl/source/command_queue/command_queue_hw.h"

#include <optional>

////////////////////////////////////////////////////////////////////////////////
// MockCommandQueue - Core implementation
////////////////////////////////////////////////////////////////////////////////

namespace NEO {
class MockCommandQueue : public CommandQueue {
  public:
    using CommandQueue::bcsEngines;
    using CommandQueue::bcsEngineTypes;
    using CommandQueue::bcsTimestampPacketContainers;
    using CommandQueue::blitEnqueueAllowed;
    using CommandQueue::blitEnqueueImageAllowed;
    using CommandQueue::bufferCpuCopyAllowed;
    using CommandQueue::device;
    using CommandQueue::gpgpuEngine;
    using CommandQueue::isCopyOnly;
    using CommandQueue::isTextureCacheFlushNeeded;
    using CommandQueue::obtainNewTimestampPacketNodes;
    using CommandQueue::overrideEngine;
    using CommandQueue::queueCapabilities;
    using CommandQueue::queueFamilyIndex;
    using CommandQueue::queueFamilySelected;
    using CommandQueue::queueIndexWithinFamily;
    using CommandQueue::requiresCacheFlushAfterWalker;
    using CommandQueue::throttle;
    using CommandQueue::timestampPacketContainer;

    void clearBcsEngines() {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
        bcsEngineTypes.clear();
    }

    void insertBcsEngine(aub_stream::EngineType bcsEngineType) {
        const auto index = NEO::EngineHelpers::getBcsIndex(bcsEngineType);
        const auto engine = &getDevice().getEngine(bcsEngineType, EngineUsage::Regular);
        bcsEngines[index] = engine;
        bcsEngineTypes.push_back(bcsEngineType);
    }

    size_t countBcsEngines() const {
        return std::count_if(bcsEngines.begin(), bcsEngines.end(), [](const EngineControl *engine) {
            return engine != nullptr;
        });
    }

    void setProfilingEnabled() {
        commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    }
    void setOoqEnabled() {
        commandQueueProperties |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    }
    MockCommandQueue() : CommandQueue(nullptr, nullptr, 0, false) {}
    MockCommandQueue(Context &context) : MockCommandQueue(&context, context.getDevice(0), nullptr, false) {}
    MockCommandQueue(Context *context, ClDevice *device, const cl_queue_properties *props, bool internalUsage)
        : CommandQueue(context, device, props, internalUsage) {
    }

    LinearStream &getCS(size_t minRequiredSize) override {
        requestedCmdStreamSize = minRequiredSize;
        return CommandQueue::getCS(minRequiredSize);
    }

    void releaseIndirectHeap(IndirectHeap::Type heap) override {
        releaseIndirectHeapCalled = true;
        CommandQueue::releaseIndirectHeap(heap);
    }

    cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t size, const void *ptr,
                              GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                              cl_event *event) override {
        writeBufferCounter++;
        writeBufferBlocking = (CL_TRUE == blockingWrite);
        writeBufferOffset = offset;
        writeBufferSize = size;
        writeBufferPtr = const_cast<void *>(ptr);
        writeMapAllocation = mapAllocation;
        return writeBufferRetValue;
    }

    WaitStatus waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
        latestTaskCountWaited = gpgpuTaskCountToWait;

        waitUntilCompleteCalledCount++;
        if (waitUntilCompleteReturnValue.has_value()) {
            return *waitUntilCompleteReturnValue;
        }

        return CommandQueue::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
    }

    WaitStatus waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
        latestTaskCountWaited = gpgpuTaskCountToWait;
        return CommandQueue::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep);
    }

    cl_int enqueueCopyImage(Image *srcImage, Image *dstImage, const size_t *srcOrigin,
                            const size_t *dstOrigin, const size_t *region,
                            cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                            cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueFillImage(Image *image, const void *fillColor,
                            const size_t *origin, const size_t *region, cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueFillBuffer(Buffer *buffer, const void *pattern,
                             size_t patternSize, size_t offset,
                             size_t size, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueKernel(Kernel *kernel, cl_uint workDim, const size_t *globalWorkOffset,
                         const size_t *globalWorkSize, const size_t *localWorkSize,
                         cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueBarrierWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                      cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueSVMMap(cl_bool blockingMap, cl_map_flags mapFlags, void *svmPtr, size_t size,
                         cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                         cl_event *event, bool externalAppCall) override { return CL_SUCCESS; }

    cl_int enqueueSVMUnmap(void *svmPtr, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                           cl_event *event, bool externalAppCall) override { return CL_SUCCESS; }

    cl_int enqueueSVMFree(cl_uint numSvmPointers, void *svmPointers[],
                          void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                         cl_uint numSvmPointers,
                                                         void *svmPointers[],
                                                         void *userData),
                          void *userData, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                          cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueSVMMemcpy(cl_bool blockingCopy, void *dstPtr, const void *srcPtr, size_t size,
                            cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueSVMMemFill(void *svmPtr, const void *pattern, size_t patternSize, size_t size, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueMigrateMemObjects(cl_uint numMemObjects, const cl_mem *memObjects, cl_mem_migration_flags flags,
                                    cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueSVMMigrateMem(cl_uint numSvmPointers, const void **svmPointers, const size_t *sizes, const cl_mem_migration_flags flags,
                                cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueCopyBuffer(Buffer *srcBuffer, Buffer *dstBuffer, size_t srcOffset, size_t dstOffset,
                             size_t size, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueReadBuffer(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t size, void *ptr,
                             GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region,
                            size_t rowPitch, size_t slicePitch, void *ptr,
                            GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite, const size_t *origin, const size_t *region,
                             size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                             cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueCopyBufferRect(Buffer *srcBuffer, Buffer *dstBuffer, const size_t *srcOrigin, const size_t *dstOrigin,
                                 const size_t *region, size_t srcRowPitch, size_t srcSlicePitch, size_t dstRowPitch,
                                 size_t dstSlicePitch, cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueWriteBufferRect(Buffer *buffer, cl_bool blockingWrite, const size_t *bufferOrigin,
                                  const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
                                  size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
                                  const void *ptr, cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueReadBufferRect(Buffer *buffer, cl_bool blockingRead, const size_t *bufferOrigin,
                                 const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
                                 size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
                                 void *ptr, cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueCopyBufferToImage(Buffer *srcBuffer, Image *dstImage, size_t srcOffset,
                                    const size_t *dstOrigin, const size_t *region, cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueCopyImageToBuffer(Image *srcImage, Buffer *dstBuffer, const size_t *srcOrigin, const size_t *region,
                                    size_t dstOffset, cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueResourceBarrier(BarrierCommand *resourceBarrier, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                  cl_event *event) override { return CL_SUCCESS; }

    cl_int finish() override { return CL_SUCCESS; }

    cl_int flush() override { return CL_SUCCESS; }

    bool obtainTimestampPacketForCacheFlush(bool isCacheFlushRequired) const override { return isCacheFlushRequired; }

    bool waitForTimestamps(Range<CopyEngineState> copyEnginesToWait, uint32_t taskCount, WaitStatus &status, TimestampPacketContainer *mainContainer, TimestampPacketContainer *deferredContainer) override {
        waitForTimestampsCalled = true;
        return false;
    };

    bool releaseIndirectHeapCalled = false;
    bool waitForTimestampsCalled = false;
    cl_int writeBufferRetValue = CL_SUCCESS;
    uint32_t writeBufferCounter = 0;
    bool writeBufferBlocking = false;
    size_t writeBufferOffset = 0;
    size_t writeBufferSize = 0;
    void *writeBufferPtr = nullptr;
    size_t requestedCmdStreamSize = 0;
    GraphicsAllocation *writeMapAllocation = nullptr;
    std::atomic<uint32_t> latestTaskCountWaited{std::numeric_limits<uint32_t>::max()};
    std::optional<WaitStatus> waitUntilCompleteReturnValue{};
    int waitUntilCompleteCalledCount{0};
};

template <typename GfxFamily>
class MockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    using BaseClass = CommandQueueHw<GfxFamily>;

  public:
    using BaseClass::bcsEngines;
    using BaseClass::bcsStates;
    using BaseClass::blitEnqueueAllowed;
    using BaseClass::commandQueueProperties;
    using BaseClass::commandStream;
    using BaseClass::deferredTimestampPackets;
    using BaseClass::gpgpuEngine;
    using BaseClass::isBlitAuxTranslationRequired;
    using BaseClass::latestSentEnqueueType;
    using BaseClass::obtainCommandStream;
    using BaseClass::obtainNewTimestampPacketNodes;
    using BaseClass::requiresCacheFlushAfterWalker;
    using BaseClass::throttle;
    using BaseClass::timestampPacketContainer;

    void clearBcsStates() {
        CopyEngineState unusedState{};
        std::fill(bcsStates.begin(), bcsStates.end(), unusedState);
    }

    MockCommandQueueHw(Context *context,
                       ClDevice *device,
                       cl_queue_properties *properties) : MockCommandQueueHw(context, device, properties, false) {}

    MockCommandQueueHw(Context *context,
                       ClDevice *device,
                       cl_queue_properties *properties, bool isInternal) : BaseClass(context, device, properties, isInternal) {
        this->constructBcsEngine(false);
    }

    void clearBcsEngines() {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
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

    cl_int enqueueWriteImage(Image *dstImage,
                             cl_bool blockingWrite,
                             const size_t *origin,
                             const size_t *region,
                             size_t inputRowPitch,
                             size_t inputSlicePitch,
                             const void *ptr,
                             GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override {
        EnqueueWriteImageCounter++;
        return BaseClass::enqueueWriteImage(dstImage,
                                            blockingWrite,
                                            origin,
                                            region,
                                            inputRowPitch,
                                            inputSlicePitch,
                                            ptr,
                                            mapAllocation,
                                            numEventsInWaitList,
                                            eventWaitList,
                                            event);
    }
    void *cpuDataTransferHandler(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &retVal) override {
        cpuDataTransferHandlerCalled = true;
        return BaseClass::cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    }
    cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t size,
                              const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override {
        EnqueueWriteBufferCounter++;
        blockingWriteBuffer = blockingWrite == CL_TRUE;
        return BaseClass::enqueueWriteBuffer(buffer, blockingWrite, offset, size, ptr, mapAllocation, numEventsInWaitList, eventWaitList, event);
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

    WaitStatus waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
        latestTaskCountWaited = gpgpuTaskCountToWait;
        if (waitUntilCompleteReturnValue.has_value()) {
            return *waitUntilCompleteReturnValue;
        }

        return BaseClass::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
    }

    WaitStatus waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList) override {
        waitForAllEnginesCalledCount++;

        if (waitForAllEnginesReturnValue.has_value()) {
            return *waitForAllEnginesReturnValue;
        }

        return BaseClass::waitForAllEngines(blockedQueue, printfHandler, cleanTemporaryAllocationsList);
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
    bool isGpgpuSubmissionForBcsRequired(bool queueBlocked, TimestampPacketDependencies &timestampPacketDependencies) const override {
        if (forceGpgpuSubmissionForBcsRequired != -1) {
            return forceGpgpuSubmissionForBcsRequired;
        }
        return BaseClass::isGpgpuSubmissionForBcsRequired(queueBlocked, timestampPacketDependencies);
    }

    unsigned int lastCommandType;
    std::vector<Kernel *> lastEnqueuedKernels;
    MultiDispatchInfo storedMultiDispatchInfo;
    size_t EnqueueWriteImageCounter = 0;
    size_t EnqueueWriteBufferCounter = 0;
    size_t requestedCmdStreamSize = 0;
    bool blockingWriteBuffer = false;
    bool storeMultiDispatchInfo = false;
    bool notifyEnqueueReadBufferCalled = false;
    bool notifyEnqueueReadImageCalled = false;
    bool notifyEnqueueSVMMemcpyCalled = false;
    bool cpuDataTransferHandlerCalled = false;
    bool useBcsCsrOnNotifyEnabled = false;
    int setQueueBlocked = -1;
    int forceGpgpuSubmissionForBcsRequired = -1;
    mutable bool isBlitEnqueueImageAllowed = false;
    struct OverrideReturnValue {
        bool enabled = false;
        bool returnValue = false;
    } overrideIsCacheFlushForBcsRequired;
    BuiltinOpParams kernelParams;
    std::atomic<uint32_t> latestTaskCountWaited{std::numeric_limits<uint32_t>::max()};
    bool flushCalled = false;
    std::optional<WaitStatus> waitForAllEnginesReturnValue{};
    std::optional<WaitStatus> waitUntilCompleteReturnValue{};
    int waitForAllEnginesCalledCount{0};

    LinearStream *peekCommandStream() {
        return this->commandStream;
    }
};
} // namespace NEO

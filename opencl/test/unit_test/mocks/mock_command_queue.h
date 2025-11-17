/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/product_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/copy_engine_state.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/properties_helper.h"
#include "opencl/source/kernel/kernel.h"

#include "CL/cl.h"
#include "CL/cl_platform.h"
#include "algorithm"
#include "aubstream/engine_node.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

////////////////////////////////////////////////////////////////////////////////
// MockCommandQueue - Core implementation
////////////////////////////////////////////////////////////////////////////////

namespace NEO {
class MockCommandQueue : public CommandQueue {
  public:
    using CommandQueue::bcsAllowed;
    using CommandQueue::bcsEngineCount;
    using CommandQueue::bcsEngines;
    using CommandQueue::bcsInitialized;
    using CommandQueue::bcsQueueEngineType;
    using CommandQueue::bcsStates;
    using CommandQueue::bcsTimestampPacketContainers;
    using CommandQueue::blitEnqueueAllowed;
    using CommandQueue::blitEnqueueImageAllowed;
    using CommandQueue::bufferCpuCopyAllowed;
    using CommandQueue::d2hEngines;
    using CommandQueue::deferredTimestampPackets;
    using CommandQueue::device;
    using CommandQueue::gpgpuEngine;
    using CommandQueue::h2dEngines;
    using CommandQueue::heaplessModeEnabled;
    using CommandQueue::heaplessStateInitEnabled;
    using CommandQueue::isCopyOnly;
    using CommandQueue::isTextureCacheFlushNeeded;
    using CommandQueue::migrateMultiGraphicsAllocationsIfRequired;
    using CommandQueue::obtainNewTimestampPacketNodes;
    using CommandQueue::overrideEngine;
    using CommandQueue::priority;
    using CommandQueue::queueCapabilities;
    using CommandQueue::queueFamilyIndex;
    using CommandQueue::queueFamilySelected;
    using CommandQueue::queueIndexWithinFamily;
    using CommandQueue::splitBarrierRequired;
    using CommandQueue::throttle;
    using CommandQueue::timestampPacketContainer;

    void clearBcsEngines() {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
        bcsQueueEngineType = std::nullopt;
    }

    void insertBcsEngine(aub_stream::EngineType bcsEngineType);

    size_t countBcsEngines() const;

    void setProfilingEnabled() {
        commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    }
    void setOoqEnabled() {
        commandQueueProperties |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    }
    MockCommandQueue();
    MockCommandQueue(Context &context);
    MockCommandQueue(Context *context, ClDevice *device, const cl_queue_properties *props, bool internalUsage);

    LinearStream &getCS(size_t minRequiredSize) override {
        requestedCmdStreamSize = minRequiredSize;
        return CommandQueue::getCS(minRequiredSize);
    }

    void releaseIndirectHeap(IndirectHeapType heap) override {
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

    cl_int enqueueWriteBufferImpl(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t cb,
                                  const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override {
        return CL_SUCCESS;
    }

    WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) override {
        latestTaskCountWaited = gpgpuTaskCountToWait;

        waitUntilCompleteCalledCount++;
        if (waitUntilCompleteReturnValue.has_value()) {
            return *waitUntilCompleteReturnValue;
        }

        return CommandQueue::waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, cleanTemporaryAllocationList, skipWait);
    }

    WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) override {
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
                            cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver *csrParam) override { return CL_SUCCESS; }

    cl_int enqueueSVMMemFill(void *svmPtr, const void *pattern, size_t patternSize, size_t size, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) override;

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

    cl_int enqueueReadBufferImpl(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t cb,
                                 void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override {
        enqueueReadBufferImplCalledCount++;
        return CL_SUCCESS;
    }

    cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region,
                            size_t rowPitch, size_t slicePitch, void *ptr,
                            GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList, cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueReadImageImpl(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region,
                                size_t rowPitch, size_t slicePitch, void *ptr,
                                GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override { return CL_SUCCESS; }

    cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite, const size_t *origin, const size_t *region,
                             size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                             cl_event *event) override { return CL_SUCCESS; }

    cl_int enqueueWriteImageImpl(Image *dstImage, cl_bool blockingWrite, const size_t *origin, const size_t *region,
                                 size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, GraphicsAllocation *mapAllocation,
                                 cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                 cl_event *event, CommandStreamReceiver &csr) override { return CL_SUCCESS; }

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

    cl_int finish(bool resolvePendingL3Flushes) override {
        ++finishCalledCount;
        return CL_SUCCESS;
    }

    cl_int flush() override { return CL_SUCCESS; }

    void programPendingL3Flushes(CommandStreamReceiver &csr, bool &waitForTaskCountRequired, bool resolvePendingL3Flushes) override {
    }

    bool waitForTimestamps(std::span<CopyEngineState> copyEnginesToWait, WaitStatus &status, TimestampPacketContainer *mainContainer, TimestampPacketContainer *deferredContainer) override {
        waitForTimestampsCalled = true;
        return false;
    };

    bool isCompleted(TaskCountType gpgpuTaskCount, std::span<const CopyEngineState> bcsStates) override;

    bool enqueueMarkerWithWaitListCalled = false;
    bool releaseIndirectHeapCalled = false;
    bool waitForTimestampsCalled = false;
    cl_int writeBufferRetValue = CL_SUCCESS;
    uint32_t finishCalledCount = 0;
    uint32_t isCompletedCalled = 0;
    uint32_t writeBufferCounter = 0;
    bool writeBufferBlocking = false;
    size_t writeBufferOffset = 0;
    size_t writeBufferSize = 0;
    void *writeBufferPtr = nullptr;
    size_t requestedCmdStreamSize = 0;
    GraphicsAllocation *writeMapAllocation = nullptr;
    std::atomic<TaskCountType> latestTaskCountWaited{std::numeric_limits<TaskCountType>::max()};
    std::optional<WaitStatus> waitUntilCompleteReturnValue{};
    int waitUntilCompleteCalledCount{0};
    size_t enqueueReadBufferImplCalledCount = 0;
};

} // namespace NEO

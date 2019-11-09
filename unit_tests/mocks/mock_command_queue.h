/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

////////////////////////////////////////////////////////////////////////////////
// MockCommandQueue - Core implementation
////////////////////////////////////////////////////////////////////////////////

namespace NEO {
class MockCommandQueue : public CommandQueue {
  public:
    using CommandQueue::device;
    using CommandQueue::gpgpuEngine;
    using CommandQueue::multiEngineQueue;
    using CommandQueue::obtainNewTimestampPacketNodes;
    using CommandQueue::requiresCacheFlushAfterWalker;
    using CommandQueue::throttle;
    using CommandQueue::timestampPacketContainer;

    void setProfilingEnabled() {
        commandQueueProperties |= CL_QUEUE_PROFILING_ENABLE;
    }
    void setOoqEnabled() {
        commandQueueProperties |= CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    }
    MockCommandQueue() : CommandQueue(nullptr, nullptr, 0) {}
    MockCommandQueue(Context &context) : MockCommandQueue(&context, context.getDevice(0), nullptr) {}
    MockCommandQueue(Context *context, Device *device, const cl_queue_properties *props)
        : CommandQueue(context, device, props) {
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
        return writeBufferRetValue;
    }

    bool releaseIndirectHeapCalled = false;

    cl_int writeBufferRetValue = CL_SUCCESS;
    uint32_t writeBufferCounter = 0;
    bool writeBufferBlocking = false;
    size_t writeBufferOffset = 0;
    size_t writeBufferSize = 0;
    void *writeBufferPtr = nullptr;
    size_t requestedCmdStreamSize = 0;
};

template <typename GfxFamily>
class MockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    using BaseClass::bcsEngine;
    using BaseClass::bcsTaskCount;
    using BaseClass::commandStream;
    using BaseClass::gpgpuEngine;
    using BaseClass::multiEngineQueue;
    using BaseClass::obtainCommandStream;
    using BaseClass::obtainNewTimestampPacketNodes;
    using BaseClass::requiresCacheFlushAfterWalker;
    using BaseClass::timestampPacketContainer;

    MockCommandQueueHw(Context *context,
                       Device *device,
                       cl_queue_properties *properties) : BaseClass(context, device, properties) {
    }

    UltCommandStreamReceiver<GfxFamily> &getUltCommandStreamReceiver() {
        return reinterpret_cast<UltCommandStreamReceiver<GfxFamily> &>(*BaseClass::gpgpuEngine->commandStreamReceiver);
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

    void notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead) override {
        notifyEnqueueReadBufferCalled = true;
    }
    void notifyEnqueueReadImage(Image *image, bool blockingRead) override {
        notifyEnqueueReadImageCalled = true;
    }

    unsigned int lastCommandType;
    std::vector<Kernel *> lastEnqueuedKernels;
    MultiDispatchInfo storedMultiDispatchInfo;
    size_t EnqueueWriteImageCounter = 0;
    size_t EnqueueWriteBufferCounter = 0;
    bool blockingWriteBuffer = false;
    bool storeMultiDispatchInfo = false;
    bool notifyEnqueueReadBufferCalled = false;
    bool notifyEnqueueReadImageCalled = false;
    bool cpuDataTransferHandlerCalled = false;
    BuiltinOpParams kernelParams;

    LinearStream *peekCommandStream() {
        return this->commandStream;
    }
};
} // namespace NEO

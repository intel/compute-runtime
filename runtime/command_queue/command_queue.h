/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/event/event.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/engine_control.h"
#include "runtime/helpers/task_information.h"

#include <atomic>
#include <cstdint>

namespace NEO {
class BarrierCommand;
class Buffer;
class LinearStream;
class Context;
class Device;
class Event;
class EventBuilder;
class FlushStampTracker;
class Image;
class IndirectHeap;
class Kernel;
class MemObj;
class PerformanceCounters;
struct CompletionStamp;
struct MultiDispatchInfo;

enum class QueuePriority {
    LOW,
    MEDIUM,
    HIGH
};

inline bool shouldFlushDC(uint32_t commandType, PrintfHandler *printfHandler) {
    return (commandType == CL_COMMAND_READ_BUFFER ||
            commandType == CL_COMMAND_READ_BUFFER_RECT ||
            commandType == CL_COMMAND_READ_IMAGE ||
            commandType == CL_COMMAND_SVM_MAP ||
            printfHandler);
}

template <>
struct OpenCLObjectMapper<_cl_command_queue> {
    typedef class CommandQueue DerivedType;
};

class CommandQueue : public BaseObject<_cl_command_queue> {
  public:
    static const cl_ulong objectMagic = 0x1234567890987654LL;

    static CommandQueue *create(Context *context, Device *device,
                                const cl_queue_properties *properties,
                                cl_int &errcodeRet);

    CommandQueue();

    CommandQueue(Context *context, Device *device,
                 const cl_queue_properties *properties);

    CommandQueue &operator=(const CommandQueue &) = delete;
    CommandQueue(const CommandQueue &) = delete;

    ~CommandQueue() override;

    // API entry points
    virtual cl_int
    enqueueCopyImage(Image *srcImage, Image *dstImage, const size_t srcOrigin[3],
                     const size_t dstOrigin[3], const size_t region[3],
                     cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueFillImage(Image *image, const void *fillColor,
                                    const size_t *origin, const size_t *region,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueFillBuffer(Buffer *buffer, const void *pattern,
                                     size_t patternSize, size_t offset,
                                     size_t size, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueKernel(cl_kernel kernel, cl_uint workDim,
                                 const size_t *globalWorkOffset,
                                 const size_t *globalWorkSize,
                                 const size_t *localWorkSize,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueBarrierWithWaitList(cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
        return CL_SUCCESS;
    }

    MOCKABLE_VIRTUAL void *enqueueMapBuffer(Buffer *buffer, cl_bool blockingMap,
                                            cl_map_flags mapFlags, size_t offset,
                                            size_t size, cl_uint numEventsInWaitList,
                                            const cl_event *eventWaitList, cl_event *event,
                                            cl_int &errcodeRet);

    MOCKABLE_VIRTUAL void *enqueueMapImage(Image *image, cl_bool blockingMap,
                                           cl_map_flags mapFlags, const size_t *origin,
                                           const size_t *region, size_t *imageRowPitch,
                                           size_t *imageSlicePitch, cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList, cl_event *event, cl_int &errcodeRet);

    MOCKABLE_VIRTUAL cl_int enqueueUnmapMemObject(MemObj *memObj, void *mappedPtr, cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList, cl_event *event);

    virtual cl_int enqueueSVMMap(cl_bool blockingMap, cl_map_flags mapFlags,
                                 void *svmPtr, size_t size,
                                 cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                 cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueSVMUnmap(void *svmPtr,
                                   cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                   cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueSVMFree(cl_uint numSvmPointers,
                                  void *svmPointers[],
                                  void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                                 cl_uint numSvmPointers,
                                                                 void *svmPointers[],
                                                                 void *userData),
                                  void *userData,
                                  cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList,
                                  cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueSVMMemcpy(cl_bool blockingCopy,
                                    void *dstPtr,
                                    const void *srcPtr,
                                    size_t size,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueSVMMemFill(void *svmPtr,
                                     const void *pattern,
                                     size_t patternSize,
                                     size_t size,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList,
                                             const cl_event *eventWaitList,
                                             cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueMigrateMemObjects(cl_uint numMemObjects,
                                            const cl_mem *memObjects,
                                            cl_mem_migration_flags flags,
                                            cl_uint numEventsInWaitList,
                                            const cl_event *eventWaitList,
                                            cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueSVMMigrateMem(cl_uint numSvmPointers,
                                        const void **svmPointers,
                                        const size_t *sizes,
                                        const cl_mem_migration_flags flags,
                                        cl_uint numEventsInWaitList,
                                        const cl_event *eventWaitList,
                                        cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueCopyBuffer(Buffer *srcBuffer, Buffer *dstBuffer,
                                     size_t srcOffset, size_t dstOffset,
                                     size_t size, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueReadBuffer(Buffer *buffer, cl_bool blockingRead,
                                     size_t offset, size_t size, void *ptr,
                                     GraphicsAllocation *mapAllocation,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead,
                                    const size_t *origin, const size_t *region,
                                    size_t rowPitch, size_t slicePitch, void *ptr,
                                    GraphicsAllocation *mapAllocation,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite,
                                      size_t offset, size_t cb, const void *ptr,
                                      GraphicsAllocation *mapAllocation,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite,
                                     const size_t *origin, const size_t *region,
                                     size_t inputRowPitch, size_t inputSlicePitch,
                                     const void *ptr, GraphicsAllocation *mapAllocation,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int
    enqueueCopyBufferRect(Buffer *srcBuffer, Buffer *dstBuffer,
                          const size_t *srcOrigin, const size_t *dstOrigin,
                          const size_t *region, size_t srcRowPitch,
                          size_t srcSlicePitch, size_t dstRowPitch,
                          size_t dstSlicePitch, cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueWriteBufferRect(
        Buffer *buffer, cl_bool blockingWrite, const size_t *bufferOrigin,
        const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
        size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
        const void *ptr, cl_uint numEventsInWaitList,
        const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueReadBufferRect(
        Buffer *buffer, cl_bool blockingRead, const size_t *bufferOrigin,
        const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
        size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
        void *ptr, cl_uint numEventsInWaitList,
        const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int
    enqueueCopyBufferToImage(Buffer *srcBuffer, Image *dstImage, size_t srcOffset,
                             const size_t *dstOrigin, const size_t *region,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int
    enqueueCopyImageToBuffer(Image *srcImage, Buffer *dstBuffer,
                             const size_t *srcOrigin, const size_t *region,
                             size_t dstOffset, cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList, cl_event *event) {
        return CL_SUCCESS;
    }

    cl_int enqueueAcquireSharedObjects(cl_uint numObjects,
                                       const cl_mem *memObjects,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *oclEvent,
                                       cl_uint cmdType);

    cl_int enqueueReleaseSharedObjects(cl_uint numObjects,
                                       const cl_mem *memObjects,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *oclEvent,
                                       cl_uint cmdType);

    MOCKABLE_VIRTUAL void *cpuDataTransferHandler(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &retVal);

    virtual cl_int enqueueResourceBarrier(BarrierCommand *resourceBarrier,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int finish(bool dcFlush) { return CL_SUCCESS; }

    virtual cl_int flush() { return CL_SUCCESS; }

    MOCKABLE_VIRTUAL void updateFromCompletionStamp(const CompletionStamp &completionStamp);

    virtual bool isCacheFlushCommand(uint32_t commandType) { return false; }

    cl_int getCommandQueueInfo(cl_command_queue_info paramName,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet);

    uint32_t getHwTag() const;

    volatile uint32_t *getHwTagAddress() const;

    bool isCompleted(uint32_t taskCount) const;

    MOCKABLE_VIRTUAL bool isQueueBlocked();

    MOCKABLE_VIRTUAL void waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep);

    static uint32_t getTaskLevelFromWaitList(uint32_t taskLevel,
                                             cl_uint numEventsInWaitList,
                                             const cl_event *eventWaitList);

    CommandStreamReceiver &getCommandStreamReceiver() const;
    Device &getDevice() const { return *device; }
    Context &getContext() const { return *context; }
    Context *getContextPtr() const { return context; }
    EngineControl &getEngine() const { return *engine; }

    MOCKABLE_VIRTUAL LinearStream &getCS(size_t minRequiredSize);
    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType,
                                  size_t minRequiredSize);

    void allocateHeapMemory(IndirectHeap::Type heapType,
                            size_t minRequiredSize, IndirectHeap *&indirectHeap);

    MOCKABLE_VIRTUAL void releaseIndirectHeap(IndirectHeap::Type heapType);

    void releaseVirtualEvent() {
        if (this->virtualEvent != nullptr) {
            this->virtualEvent->decRefInternal();
            this->virtualEvent = nullptr;
        }
    }

    cl_command_queue_properties getCommandQueueProperties() const {
        return commandQueueProperties;
    }

    bool isProfilingEnabled() const {
        return !!(this->getCommandQueueProperties() & CL_QUEUE_PROFILING_ENABLE);
    }

    bool isOOQEnabled() const {
        return !!(this->getCommandQueueProperties() & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    }

    bool isPerfCountersEnabled() const {
        return perfCountersEnabled;
    }

    PerformanceCounters *getPerfCounters();

    bool setPerfCountersEnabled(bool perfCountersEnabled, cl_uint configuration);

    void setIsSpecialCommandQueue(bool newValue) {
        this->isSpecialCommandQueue = newValue;
    }

    QueuePriority getPriority() const {
        return priority;
    }

    QueueThrottle getThrottle() const {
        return throttle;
    }

    void enqueueBlockedMapUnmapOperation(const cl_event *eventWaitList,
                                         size_t numEventsInWaitlist,
                                         MapOperationType opType,
                                         MemObj *memObj,
                                         MemObjSizeArray &copySize,
                                         MemObjOffsetArray &copyOffset,
                                         bool readOnly,
                                         EventBuilder &externalEventBuilder);

    MOCKABLE_VIRTUAL bool setupDebugSurface(Kernel *kernel);

    bool getRequiresCacheFlushAfterWalker() const {
        return requiresCacheFlushAfterWalker;
    }

    bool isMultiEngineQueue() const { return this->multiEngineQueue; }

    // taskCount of last task
    uint32_t taskCount = 0;

    // current taskLevel. Used for determining if a PIPE_CONTROL is needed.
    uint32_t taskLevel = 0;

    std::unique_ptr<FlushStampTracker> flushStamp;

    std::atomic<uint32_t> latestTaskCountWaited{std::numeric_limits<uint32_t>::max()};

    // virtual event that holds last Enqueue information
    Event *virtualEvent = nullptr;

    size_t estimateTimestampPacketNodesCount(const MultiDispatchInfo &dispatchInfo) const;

  protected:
    void *enqueueReadMemObjForMap(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest);

    void *enqueueMapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueUnmapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest);

    virtual void obtainTaskLevelAndBlockedStatus(unsigned int &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueueStatus, unsigned int commandType){};

    MOCKABLE_VIRTUAL void obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies);
    void processProperties(const cl_queue_properties *properties);
    bool bufferCpuCopyAllowed(Buffer *buffer, cl_command_type commandType, cl_bool blocking, size_t size, void *ptr,
                              cl_uint numEventsInWaitList, const cl_event *eventWaitList);
    void providePerformanceHint(TransferProperties &transferProperties);
    bool queueDependenciesClearRequired() const;
    bool blitEnqueueAllowed(bool queueBlocked, cl_command_type cmdType);

    Context *context = nullptr;
    Device *device = nullptr;
    EngineControl *engine = nullptr;

    cl_command_queue_properties commandQueueProperties = 0;

    QueuePriority priority = QueuePriority::MEDIUM;
    QueueThrottle throttle = QueueThrottle::MEDIUM;

    bool perfCountersEnabled = false;

    LinearStream *commandStream = nullptr;

    bool mapDcFlushRequired = false;
    bool isSpecialCommandQueue = false;
    bool requiresCacheFlushAfterWalker = false;
    bool multiEngineQueue = false;

    std::unique_ptr<TimestampPacketContainer> timestampPacketContainer;
};

typedef CommandQueue *(*CommandQueueCreateFunc)(
    Context *context, Device *device, const cl_queue_properties *properties);

template <typename GfxFamily, unsigned int eventType>
LinearStream &getCommandStream(CommandQueue &commandQueue,
                               bool reserveProfilingCmdsSpace,
                               bool reservePerfCounterCmdsSpace,
                               const Kernel *pKernel);

template <typename GfxFamily, IndirectHeap::Type heapType>
IndirectHeap &getIndirectHeap(CommandQueue &commandQueue, const Kernel &kernel);
} // namespace NEO

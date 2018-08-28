/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/event/user_event.h"
#include "runtime/os_interface/performance_counters.h"
#include <atomic>
#include <cstdint>

namespace OCLRT {
class Buffer;
class LinearStream;
class Context;
class Device;
class EventBuilder;
class Image;
class IndirectHeap;
class Kernel;
class MemObj;
class TimestampPacket;
struct CompletionStamp;

enum class QueuePriority {
    LOW,
    MEDIUM,
    HIGH
};

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
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead,
                                    const size_t *origin, const size_t *region,
                                    size_t rowPitch, size_t slicePitch, void *ptr,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite,
                                      size_t offset, size_t cb, const void *ptr,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
        return CL_SUCCESS;
    }

    virtual cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite,
                                     const size_t *origin, const size_t *region,
                                     size_t inputRowPitch, size_t inputSlicePitch,
                                     const void *ptr, cl_uint numEventsInWaitList,
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

    void *cpuDataTransferHandler(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &retVal);

    virtual cl_int finish(bool dcFlush) { return CL_SUCCESS; }

    virtual cl_int flush() { return CL_SUCCESS; }

    void updateFromCompletionStamp(const CompletionStamp &completionStamp);

    cl_int getCommandQueueInfo(cl_command_queue_info paramName,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet);

    uint32_t getHwTag() const;

    volatile uint32_t *getHwTagAddress() const;

    bool isCompleted(uint32_t taskCount) const;

    MOCKABLE_VIRTUAL bool isQueueBlocked();

    MOCKABLE_VIRTUAL void waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep);

    void flushWaitList(cl_uint numEventsInWaitList,
                       const cl_event *eventWaitList,
                       bool ndRangeKernel);

    static uint32_t getTaskLevelFromWaitList(uint32_t taskLevel,
                                             cl_uint numEventsInWaitList,
                                             const cl_event *eventWaitList);

    Device &getDevice() { return *device; }
    Context &getContext() { return *context; }
    Context *getContextPtr() { return context; }

    MOCKABLE_VIRTUAL LinearStream &getCS(size_t minRequiredSize);
    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType,
                                  size_t minRequiredSize);

    void allocateHeapMemory(IndirectHeap::Type heapType,
                            size_t minRequiredSize, IndirectHeap *&indirectHeap);

    MOCKABLE_VIRTUAL void releaseIndirectHeap(IndirectHeap::Type heapType);

    cl_command_queue_properties getCommandQueueProperties() const {
        return commandQueueProperties;
    }

    bool isProfilingEnabled() {
        return !!(this->getCommandQueueProperties() & CL_QUEUE_PROFILING_ENABLE);
    }

    bool isOOQEnabled() {
        return !!(this->getCommandQueueProperties() & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    }

    bool isPerfCountersEnabled() {
        return perfCountersEnabled;
    }

    InstrPmRegsCfg *getPerfCountersConfigData() {
        return perfConfigurationData;
    }

    PerformanceCounters *getPerfCounters();

    bool sendPerfCountersConfig();

    bool setPerfCountersEnabled(bool perfCountersEnabled, cl_uint configuration);

    void setIsSpecialCommandQueue(bool newValue) {
        this->isSpecialCommandQueue = newValue;
    }

    uint16_t getPerfCountersUserRegistersNumber() {
        return perfCountersUserRegistersNumber;
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

    // taskCount of last task
    uint32_t taskCount;

    // current taskLevel. Used for determining if a PIPE_CONTROL is needed.
    uint32_t taskLevel;

    std::unique_ptr<FlushStampTracker> flushStamp;

    std::atomic<uint32_t> latestTaskCountWaited{(uint32_t)-1};

    // virtual event that holds last Enqueue information
    Event *virtualEvent;

  protected:
    void *enqueueReadMemObjForMap(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest);

    void *enqueueMapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueUnmapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest);

    virtual void obtainTaskLevelAndBlockedStatus(unsigned int &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueue, unsigned int commandType){};

    MOCKABLE_VIRTUAL void dispatchAuxTranslation(MultiDispatchInfo &multiDispatchInfo, BuffersForAuxTranslation &buffersForAuxTranslation,
                                                 AuxTranslationDirection auxTranslationDirection);

    void obtainNewTimestampPacketNode();

    Context *context;
    Device *device;

    cl_command_queue_properties commandQueueProperties;

    QueuePriority priority;
    QueueThrottle throttle;

    bool perfCountersEnabled;
    cl_uint perfCountersConfig;
    uint32_t perfCountersUserRegistersNumber;
    InstrPmRegsCfg *perfConfigurationData;
    uint32_t perfCountersRegsCfgHandle;
    bool perfCountersRegsCfgPending;

    LinearStream *commandStream;

    bool mapDcFlushRequired = false;
    bool isSpecialCommandQueue = false;

    TagNode<TimestampPacket> *timestampPacketNode = nullptr;

  private:
    void providePerformanceHint(TransferProperties &transferProperties);
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
} // namespace OCLRT

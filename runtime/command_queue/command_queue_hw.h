/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/program/printf_handler.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/queue_helpers.h"
#include <memory>

namespace OCLRT {

class EventBuilder;

template <typename GfxFamily>
class CommandQueueHw : public CommandQueue {
    typedef CommandQueue BaseClass;

  public:
    CommandQueueHw(Context *context,
                   Device *device,
                   const cl_queue_properties *properties) : BaseClass(context, device, properties) {

        auto clPriority = getCmdQueueProperties<cl_queue_priority_khr>(properties, CL_QUEUE_PRIORITY_KHR);

        if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_LOW_KHR)) {
            priority = QueuePriority::LOW;
        } else if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_MED_KHR)) {
            priority = QueuePriority::MEDIUM;
        } else if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_HIGH_KHR)) {
            priority = QueuePriority::HIGH;
        }

        auto clThrottle = getCmdQueueProperties<cl_queue_throttle_khr>(properties, CL_QUEUE_THROTTLE_KHR);

        if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_LOW_KHR)) {
            throttle = QueueThrottle::LOW;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_MED_KHR)) {
            throttle = QueueThrottle::MEDIUM;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_HIGH_KHR)) {
            throttle = QueueThrottle::HIGH;
        }

        if (getCmdQueueProperties<cl_queue_properties>(properties, CL_QUEUE_PROPERTIES) & static_cast<cl_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)) {
            device->getCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::BatchedDispatch);
            device->getCommandStreamReceiver().enableNTo1SubmissionModel();
        }
    }

    static CommandQueue *create(Context *context,
                                Device *device,
                                const cl_queue_properties *properties) {
        return new CommandQueueHw<GfxFamily>(context, device, properties);
    }

    MOCKABLE_VIRTUAL void notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead);
    MOCKABLE_VIRTUAL void notifyEnqueueReadImage(Image *image, bool blockingRead);

    cl_int enqueueBarrierWithWaitList(cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) override;

    cl_int enqueueCopyBuffer(Buffer *srcBuffer,
                             Buffer *dstBuffer,
                             size_t srcOffset,
                             size_t dstOffset,
                             size_t size,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueCopyBufferRect(Buffer *srcBuffer,
                                 Buffer *dstBuffer,
                                 const size_t *srcOrigin,
                                 const size_t *dstOrigin,
                                 const size_t *region,
                                 size_t srcRowPitch,
                                 size_t srcSlicePitch,
                                 size_t dstRowPitch,
                                 size_t dstSlicePitch,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) override;

    cl_int enqueueCopyImage(Image *srcImage,
                            Image *dstImage,
                            const size_t srcOrigin[3],
                            const size_t dstOrigin[3],
                            const size_t region[3],
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event) override;

    cl_int enqueueFillBuffer(Buffer *buffer,
                             const void *pattern,
                             size_t patternSize,
                             size_t offset,
                             size_t size,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueFillImage(Image *image,
                            const void *fillColor,
                            const size_t *origin,
                            const size_t *region,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event) override;

    cl_int enqueueKernel(cl_kernel kernel,
                         cl_uint workDim,
                         const size_t *globalWorkOffset,
                         const size_t *globalWorkSize,
                         const size_t *localWorkSize,
                         cl_uint numEventsInWaitList,
                         const cl_event *eventWaitList,
                         cl_event *event) override;

    cl_int enqueueSVMMap(cl_bool blockingMap,
                         cl_map_flags mapFlags,
                         void *svmPtr,
                         size_t size,
                         cl_uint numEventsInWaitList,
                         const cl_event *eventWaitList,
                         cl_event *event) override;

    cl_int enqueueSVMUnmap(void *svmPtr,
                           cl_uint numEventsInWaitList,
                           const cl_event *eventWaitList,
                           cl_event *event) override;

    cl_int enqueueSVMFree(cl_uint numSvmPointers,
                          void *svmPointers[],
                          void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                         cl_uint numSvmPointers,
                                                         void *svmPointers[],
                                                         void *userData),
                          void *userData,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event) override;

    cl_int enqueueSVMMemcpy(cl_bool blockingCopy,
                            void *dstPtr,
                            const void *srcPtr,
                            size_t size,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event) override;

    cl_int enqueueSVMMemFill(void *svmPtr,
                             const void *pattern,
                             size_t patternSize,
                             size_t size,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) override;

    cl_int enqueueMigrateMemObjects(cl_uint numMemObjects,
                                    const cl_mem *memObjects,
                                    cl_mem_migration_flags flags,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) override;

    cl_int enqueueSVMMigrateMem(cl_uint numSvmPointers,
                                const void **svmPointers,
                                const size_t *sizes,
                                const cl_mem_migration_flags flags,
                                cl_uint numEventsInWaitList,
                                const cl_event *eventWaitList,
                                cl_event *event) override;

    cl_int enqueueReadBuffer(Buffer *buffer,
                             cl_bool blockingRead,
                             size_t offset,
                             size_t size,
                             void *ptr,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueReadBufferRect(Buffer *buffer,
                                 cl_bool blockingRead,
                                 const size_t *bufferOrigin,
                                 const size_t *hostOrigin,
                                 const size_t *region,
                                 size_t bufferRowPitch,
                                 size_t bufferSlicePitch,
                                 size_t hostRowPitch,
                                 size_t hostSlicePitch,
                                 void *ptr,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) override;

    cl_int enqueueReadImage(Image *srcImage,
                            cl_bool blockingRead,
                            const size_t *origin,
                            const size_t *region,
                            size_t rowPitch,
                            size_t slicePitch,
                            void *ptr,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event) override;

    cl_int enqueueWriteBuffer(Buffer *buffer,
                              cl_bool blockingWrite,
                              size_t offset,
                              size_t cb,
                              const void *ptr,
                              cl_uint numEventsInWaitList,
                              const cl_event *eventWaitList,
                              cl_event *event) override;

    cl_int enqueueWriteBufferRect(Buffer *buffer,
                                  cl_bool blockingWrite,
                                  const size_t *bufferOrigin,
                                  const size_t *hostOrigin,
                                  const size_t *region,
                                  size_t bufferRowPitch,
                                  size_t bufferSlicePitch,
                                  size_t hostRowPitch,
                                  size_t hostSlicePitch,
                                  const void *ptr,
                                  cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList,
                                  cl_event *event) override;

    cl_int enqueueWriteImage(Image *dstImage,
                             cl_bool blockingWrite,
                             const size_t *origin,
                             const size_t *region,
                             size_t inputRowPitch,
                             size_t inputSlicePitch,
                             const void *ptr,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueCopyBufferToImage(Buffer *srcBuffer,
                                    Image *dstImage,
                                    size_t srcOffset,
                                    const size_t *dstOrigin,
                                    const size_t *region,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) override;

    cl_int enqueueCopyImageToBuffer(Image *srcImage,
                                    Buffer *dstBuffer,
                                    const size_t *srcOrigin,
                                    const size_t *region,
                                    size_t dstOffset,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) override;
    cl_int finish(bool dcFlush) override;
    cl_int flush() override;

    template <uint32_t enqueueType>
    void enqueueHandler(Surface **surfacesForResidency,
                        size_t numSurfaceForResidency,
                        bool blocking,
                        const MultiDispatchInfo &dispatchInfo,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        cl_event *event);

    template <uint32_t enqueueType, size_t size>
    void enqueueHandler(Surface *(&surfacesForResidency)[size],
                        bool blocking,
                        const MultiDispatchInfo &dispatchInfo,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        cl_event *event) {
        enqueueHandler<enqueueType>(surfacesForResidency, size, blocking, dispatchInfo, numEventsInWaitList, eventWaitList, event);
    }

    template <uint32_t enqueueType, size_t size>
    void enqueueHandler(Surface *(&surfacesForResidency)[size],
                        bool blocking,
                        Kernel *kernel,
                        cl_uint workDim,
                        const size_t globalOffsets[3],
                        const size_t workItems[3],
                        const size_t *localWorkSizesIn,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        cl_event *event);

    template <uint32_t commandType>
    CompletionStamp enqueueNonBlocked(Surface **surfacesForResidency,
                                      size_t surfaceCount,
                                      LinearStream &commandStream,
                                      size_t commandStreamStart,
                                      bool &blocking,
                                      const MultiDispatchInfo &multiDispatchInfo,
                                      TimestampPacketContainer *previousTimestampPacketNodes,
                                      EventsRequest &eventsRequest,
                                      EventBuilder &eventBuilder,
                                      uint32_t taskLevel,
                                      bool slmUsed,
                                      PrintfHandler *printfHandler);

    template <uint32_t commandType>
    void enqueueBlocked(Surface **surfacesForResidency,
                        size_t surfacesCount,
                        bool &blocking,
                        const MultiDispatchInfo &multiDispatchInfo,
                        TimestampPacketContainer *previousTimestampPacketNodes,
                        KernelOperation *blockedCommandsData,
                        EventsRequest &eventsRequest,
                        bool slmUsed,
                        EventBuilder &externalEventBuilder,
                        std::unique_ptr<PrintfHandler> printfHandler);

  protected:
    MOCKABLE_VIRTUAL void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo){};
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface);
    size_t calculateHostPtrSizeForImage(size_t *region, size_t rowPitch, size_t slicePitch, Image *image);

  private:
    bool isTaskLevelUpdateRequired(const uint32_t &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType);
    void obtainTaskLevelAndBlockedStatus(unsigned int &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueue, unsigned int commandType) override;
    void forceDispatchScheduler(OCLRT::MultiDispatchInfo &multiDispatchInfo);
    static void computeOffsetsValueForRectCommands(size_t *bufferOffset,
                                                   size_t *hostOffset,
                                                   const size_t *bufferOrigin,
                                                   const size_t *hostOrigin,
                                                   const size_t *region,
                                                   size_t bufferRowPitch,
                                                   size_t bufferSlicePitch,
                                                   size_t hostRowPitch,
                                                   size_t hostSlicePitch);
};
} // namespace OCLRT

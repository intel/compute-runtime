/*
 * Copyright (c) 2017, Intel Corporation
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
        if (getCmdQueueProperties<cl_queue_priority_khr>(properties, CL_QUEUE_PRIORITY_KHR) & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_LOW_KHR)) {
            low_priority = true;
        }
        if (getCmdQueueProperties<cl_queue_properties>(properties, CL_QUEUE_PROPERTIES) & static_cast<cl_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)) {
            device->getCommandStreamReceiver().overrideDispatchPolicy(CommandStreamReceiver::BatchedDispatch);
        }
    }

    static CommandQueue *create(Context *context,
                                Device *device,
                                const cl_queue_properties *properties) {
        return new CommandQueueHw<GfxFamily>(context, device, properties);
    }

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

    void *enqueueMapBuffer(Buffer *buffer, cl_bool blockingMap, cl_map_flags mapFlags,
                           size_t offset, size_t size, cl_uint numEventsInWaitList,
                           const cl_event *eventWaitList, cl_event *event, cl_int &errcodeRet) override;

    void *enqueueMapSharedBuffer(Buffer *buffer, cl_bool blockingMap, cl_map_flags mapFlags,
                                 size_t offset, size_t size, cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event, cl_int &errcodeRet);

    void *enqueueMapImage(cl_mem image,
                          cl_bool blockingMap,
                          cl_map_flags mapFlags,
                          const size_t *origin,
                          const size_t *region,
                          size_t *imageRowPitch,
                          size_t *imageSlicePitch,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event,
                          cl_int &errcodeRet) override;

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

    cl_int enqueueUnmapMemObject(MemObj *memObj,
                                 void *mappedPtr,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) override {
        cl_int retVal;
        if (memObj->allowTiling() || memObj->peekSharingHandler()) {
            retVal = memObj->unmapObj(this, mappedPtr, numEventsInWaitList, eventWaitList, event);
        } else {
            cpuDataTransferHandler(memObj,
                                   CL_COMMAND_UNMAP_MEM_OBJECT,
                                   CL_FALSE,
                                   0,
                                   0,
                                   mappedPtr,
                                   numEventsInWaitList,
                                   eventWaitList,
                                   event,
                                   retVal);
        }
        return retVal;
    }

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

    template <unsigned int enqueueType>
    void enqueueHandler(Surface **surfacesForResidency,
                        size_t numSurfaceForResidency,
                        bool blocking,
                        const MultiDispatchInfo &dispatchInfo,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        cl_event *event);

    template <unsigned int enqueueType, size_t size>
    void enqueueHandler(Surface *(&surfacesForResidency)[size],
                        bool blocking,
                        const MultiDispatchInfo &dispatchInfo,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        cl_event *event) {
        enqueueHandler<enqueueType>(surfacesForResidency, size, blocking, dispatchInfo, numEventsInWaitList, eventWaitList, event);
    }

    template <unsigned int enqueueType, size_t size>
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

    template <unsigned int commandType>
    CompletionStamp enqueueNonBlocked(Surface **surfacesForResidency,
                                      size_t surfaceCount,
                                      LinearStream &commandStream,
                                      size_t commandStreamStart,
                                      bool &blocking,
                                      const MultiDispatchInfo &multiDispatchInfo,
                                      EventBuilder &eventBuilder,
                                      uint32_t taskLevel,
                                      bool slmUsed,
                                      PrintfHandler *printfHandler);

    template <unsigned int commandType>
    void enqueueBlocked(Surface **surfacesForResidency,
                        size_t surfacesCount,
                        bool &blocking,
                        const MultiDispatchInfo &multiDispatchInfo,
                        KernelOperation *blockedCommandsData,
                        cl_uint numEventsInWaitList,
                        const cl_event *eventWaitList,
                        bool slmUsed,
                        EventBuilder &externalEventBuilder,
                        std::unique_ptr<PrintfHandler> printfHandler);

    void addMapUnmapToWaitlistEventsDependencies(const cl_event *eventWaitList,
                                                 size_t numEventsInWaitlist,
                                                 MapOperationType opType,
                                                 MemObj *memObj,
                                                 EventBuilder &externalEventBuilder);

    void *cpuDataTransferHandler(MemObj *memObj,
                                 cl_command_type cmdType,
                                 cl_bool blocking,
                                 size_t offset,
                                 size_t size,
                                 void *ptr,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event,
                                 cl_int &retVal);

  protected:
    MOCKABLE_VIRTUAL void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo);

  private:
    bool isTaskLevelUpdateRequired(const uint32_t &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType);

    void forceDispatchScheduler(OCLRT::MultiDispatchInfo &multiDispatchInfo);
};
} // namespace OCLRT

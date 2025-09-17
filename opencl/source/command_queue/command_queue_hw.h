/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/bcs_ccs_dependency_pair_container.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/csr_selection_args.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/program/printf_handler.h"

#include <memory>

namespace NEO {

class EventBuilder;
struct EnqueueProperties;
struct KernelOperation;

template <typename GfxFamily>
class CommandQueueHw : public CommandQueue {
    using BaseClass = CommandQueue;

  public:
    ~CommandQueueHw() override;

    CommandQueueHw(Context *context,
                   ClDevice *device,
                   const cl_queue_properties *properties,
                   bool internalUsage) : BaseClass(context, device, properties, internalUsage) {
        if (debugManager.flags.SplitBcsSize.get() != -1) {
            this->minimalSizeForBcsSplit = debugManager.flags.SplitBcsSize.get() * MemoryConstants::kiloByte;
        }

        auto clPriority = getCmdQueueProperties<cl_queue_priority_khr>(properties, CL_QUEUE_PRIORITY_KHR);

        if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_LOW_KHR)) {
            priority = QueuePriority::low;
            this->gpgpuEngine = &device->getNearestGenericSubDevice(0)->getEngine(getChosenEngineType(device->getHardwareInfo()), EngineUsage::lowPriority);

        } else if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_MED_KHR)) {
            priority = QueuePriority::medium;
        } else if (clPriority & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_HIGH_KHR)) {
            priority = QueuePriority::high;
        }

        auto clThrottle = getCmdQueueProperties<cl_queue_throttle_khr>(properties, CL_QUEUE_THROTTLE_KHR);

        if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_LOW_KHR)) {
            throttle = QueueThrottle::LOW;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_MED_KHR)) {
            throttle = QueueThrottle::MEDIUM;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_HIGH_KHR)) {
            throttle = QueueThrottle::HIGH;
        }

        if (internalUsage) {
            this->gpgpuEngine = &device->getInternalEngine();
        }

        if (gpgpuEngine) {
            this->initializeGpgpuInternals();
        }

        uint64_t requestedSliceCount = getCmdQueueProperties<cl_command_queue_properties>(properties, CL_QUEUE_SLICE_COUNT_INTEL);
        if (requestedSliceCount > 0) {
            sliceCount = requestedSliceCount;
        }

        auto initializeGpgpu = false;

        if (debugManager.flags.DeferCmdQGpgpuInitialization.get() != -1) {
            initializeGpgpu = !debugManager.flags.DeferCmdQGpgpuInitialization.get();
        }

        if (initializeGpgpu) {
            this->initializeGpgpu();
        }

        for (const EngineControl *engine : bcsEngines) {
            if (engine != nullptr) {
                engine->osContext->ensureContextInitialized(false);
                engine->commandStreamReceiver->initDirectSubmission();
            }
        }

        bcsEngineCount = GfxFamily::bcsEngineCount;
    }

    static CommandQueue *create(Context *context,
                                ClDevice *device,
                                const cl_queue_properties *properties,
                                bool internalUsage) {
        return new CommandQueueHw<GfxFamily>(context, device, properties, internalUsage);
    }

    MOCKABLE_VIRTUAL void notifyEnqueueReadBuffer(Buffer *buffer, bool blockingRead, bool notifyBcsCsr);
    MOCKABLE_VIRTUAL void notifyEnqueueReadImage(Image *image, bool blockingRead, bool notifyBcsCsr);
    MOCKABLE_VIRTUAL void notifyEnqueueSVMMemcpy(GraphicsAllocation *gfxAllocation, bool blockingCopy, bool notifyBcsCsr);

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
                            const size_t *srcOrigin,
                            const size_t *dstOrigin,
                            const size_t *region,
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

    cl_int enqueueKernel(Kernel *kernel,
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
                         cl_event *event,
                         bool externalAppCall) override;

    cl_int enqueueSVMUnmap(void *svmPtr,
                           cl_uint numEventsInWaitList,
                           const cl_event *eventWaitList,
                           cl_event *event,
                           bool externalAppCall) override;

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
                            cl_event *event, CommandStreamReceiver *csrParam) override;

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
                             GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueReadBufferImpl(Buffer *buffer,
                                 cl_bool blockingRead,
                                 size_t offset,
                                 size_t size,
                                 void *ptr,
                                 GraphicsAllocation *mapAllocation,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event, CommandStreamReceiver &csr) override;

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
                            GraphicsAllocation *mapAllocation,
                            cl_uint numEventsInWaitList,
                            const cl_event *eventWaitList,
                            cl_event *event) override;

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
                                cl_event *event, CommandStreamReceiver &csr) override;

    cl_int enqueueWriteBuffer(Buffer *buffer,
                              cl_bool blockingWrite,
                              size_t offset,
                              size_t cb,
                              const void *ptr,
                              GraphicsAllocation *mapAllocation,
                              cl_uint numEventsInWaitList,
                              const cl_event *eventWaitList,
                              cl_event *event) override;

    cl_int enqueueWriteBufferImpl(Buffer *buffer,
                                  cl_bool blockingWrite,
                                  size_t offset,
                                  size_t cb,
                                  const void *ptr,
                                  GraphicsAllocation *mapAllocation,
                                  cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList,
                                  cl_event *event, CommandStreamReceiver &csr) override;

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
                             GraphicsAllocation *mapAllocation,
                             cl_uint numEventsInWaitList,
                             const cl_event *eventWaitList,
                             cl_event *event) override;

    cl_int enqueueWriteImageImpl(Image *dstImage, cl_bool blockingWrite, const size_t *origin,
                                 const size_t *region, size_t inputRowPitch, size_t inputSlicePitch,
                                 const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) override;

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

    cl_int enqueueResourceBarrier(BarrierCommand *resourceBarrier,
                                  cl_uint numEventsInWaitList,
                                  const cl_event *eventWaitList,
                                  cl_event *event) override;

    cl_int finish(bool resolvePendingL3Flushes) override;
    cl_int flush() override;

    void programPendingL3Flushes(CommandStreamReceiver &csr, bool &waitForTaskCountRequired, bool resolvePendingL3Flushes) override;

    template <uint32_t enqueueType>
    cl_int enqueueHandler(Surface **surfacesForResidency,
                          size_t numSurfaceForResidency,
                          bool blocking,
                          const MultiDispatchInfo &dispatchInfo,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event);

    template <uint32_t enqueueType, size_t size>
    cl_int enqueueHandler(Surface *(&surfacesForResidency)[size],
                          bool blocking,
                          const MultiDispatchInfo &dispatchInfo,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event) {
        return enqueueHandler<enqueueType>(surfacesForResidency, size, blocking, dispatchInfo, numEventsInWaitList, eventWaitList, event);
    }

    template <uint32_t enqueueType, size_t size>
    cl_int enqueueHandler(Surface *(&surfacesForResidency)[size],
                          bool blocking,
                          Kernel *kernel,
                          cl_uint workDim,
                          const size_t globalOffsets[3],
                          const size_t workItems[3],
                          const size_t *localWorkSizesIn,
                          const size_t *enqueuedWorkSizes,
                          cl_uint numEventsInWaitList,
                          const cl_event *eventWaitList,
                          cl_event *event);

    template <uint32_t cmdType, size_t surfaceCount>
    cl_int dispatchBcsOrGpgpuEnqueue(MultiDispatchInfo &dispatchInfo, Surface *(&surfaces)[surfaceCount], EBuiltInOps::Type builtInOperation, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &csr);

    template <uint32_t cmdType>
    cl_int enqueueBlit(const MultiDispatchInfo &multiDispatchInfo, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &bcsCsr, EventBuilder *pExternalEventBuilder);

    bool isSplitEnqueueBlitNeeded(TransferDirection transferDirection, size_t transferSize, CommandStreamReceiver &csr);
    size_t getTotalSizeFromRectRegion(const size_t *region);

    template <uint32_t cmdType>
    cl_int enqueueBlitSplit(MultiDispatchInfo &dispatchInfo, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &csr);

    MOCKABLE_VIRTUAL CompletionStamp enqueueNonBlocked(Surface **surfacesForResidency,
                                                       size_t surfaceCount,
                                                       LinearStream &commandStream,
                                                       size_t commandStreamStart,
                                                       bool &blocking,
                                                       bool clearDependenciesForSubCapture,
                                                       const MultiDispatchInfo &multiDispatchInfo,
                                                       const EnqueueProperties &enqueueProperties,
                                                       TimestampPacketDependencies &timestampPacketDependencies,
                                                       EventsRequest &eventsRequest,
                                                       EventBuilder &eventBuilder,
                                                       TaskCountType taskLevel,
                                                       PrintfHandler *printfHandler,
                                                       bool relaxedOrderingEnabled,
                                                       uint32_t commandType);

    void enqueueBlocked(uint32_t commandType,
                        Surface **surfacesForResidency,
                        size_t surfacesCount,
                        const MultiDispatchInfo &multiDispatchInfo,
                        TimestampPacketDependencies &timestampPacketDependencies,
                        std::unique_ptr<KernelOperation> &blockedCommandsData,
                        const EnqueueProperties &enqueueProperties,
                        EventsRequest &eventsRequest,
                        EventBuilder &externalEventBuilder,
                        std::unique_ptr<PrintfHandler> &&printfHandler,
                        CommandStreamReceiver *bcsCsr,
                        TagNodeBase *multiRootDeviceSyncNode,
                        CsrDependencyContainer *csrDependencies);

    MOCKABLE_VIRTUAL CompletionStamp enqueueCommandWithoutKernel(Surface **surfaces,
                                                                 size_t surfaceCount,
                                                                 LinearStream *commandStream,
                                                                 size_t commandStreamStart,
                                                                 bool &blocking,
                                                                 const EnqueueProperties &enqueueProperties,
                                                                 TimestampPacketDependencies &timestampPacketDependencies,
                                                                 EventsRequest &eventsRequest,
                                                                 EventBuilder &eventBuilder,
                                                                 TaskCountType taskLevel,
                                                                 CsrDependencies &csrDeps,
                                                                 CommandStreamReceiver *bcsCsr,
                                                                 bool hasRelaxedOrderingDependencies);
    void processDispatchForCacheFlush(Surface **surfaces,
                                      size_t numSurfaces,
                                      LinearStream *commandStream,
                                      CsrDependencies &csrDeps);
    void processDispatchForMarker(CommandQueue &commandQueue,
                                  LinearStream *commandStream,
                                  EventsRequest &eventsRequest,
                                  CsrDependencies &csrDeps);
    void processDispatchForMarkerWithTimestampPacket(CommandQueue &commandQueue,
                                                     LinearStream *commandStream,
                                                     EventsRequest &eventsRequest,
                                                     CsrDependencies &csrDeps);
    MOCKABLE_VIRTUAL BlitProperties processDispatchForBlitEnqueue(CommandStreamReceiver &blitCommandStreamReceiver,
                                                                  const MultiDispatchInfo &multiDispatchInfo,
                                                                  TimestampPacketDependencies &timestampPacketDependencies,
                                                                  const EventsRequest &eventsRequest,
                                                                  LinearStream *commandStream,
                                                                  uint32_t commandType, bool queueBlocked, bool profilingEnabled, TagNodeBase *multiRootDeviceEventSync);
    void submitCacheFlush(Surface **surfaces,
                          size_t numSurfaces,
                          LinearStream *commandStream,
                          uint64_t postSyncAddress);

    bool isCacheFlushCommand(uint32_t commandType) const override;

    bool waitForTimestamps(std::span<CopyEngineState> copyEnginesToWait, WaitStatus &status, TimestampPacketContainer *mainContainer, TimestampPacketContainer *deferredContainer) override;

    MOCKABLE_VIRTUAL bool isCacheFlushForBcsRequired() const;
    MOCKABLE_VIRTUAL void processSignalMultiRootDeviceNode(LinearStream *commandStream,
                                                           TagNodeBase *node);

  protected:
    MOCKABLE_VIRTUAL void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo){};
    MOCKABLE_VIRTUAL bool prepareCsrDependency(CsrDependencies &csrDeps, CsrDependencyContainer &dependencyTags, TimestampPacketDependencies &timestampPacketDependencies, TagAllocatorBase *allocator, bool blockQueue);

    cl_int enqueueReadWriteBufferOnCpuWithMemoryTransfer(cl_command_type commandType, Buffer *buffer,
                                                         size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                         const cl_event *eventWaitList, cl_event *event);
    cl_int enqueueReadWriteBufferOnCpuWithoutMemoryTransfer(cl_command_type commandType, Buffer *buffer,
                                                            size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                            const cl_event *eventWaitList, cl_event *event);
    cl_int enqueueMarkerForReadWriteOperation(MemObj *memObj, void *ptr, cl_command_type commandType, cl_bool blocking, cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList, cl_event *event);

    MOCKABLE_VIRTUAL void dispatchAuxTranslationBuiltin(MultiDispatchInfo &multiDispatchInfo, AuxTranslationDirection auxTranslationDirection);
    void setupBlitAuxTranslation(MultiDispatchInfo &multiDispatchInfo);

    MOCKABLE_VIRTUAL bool forceStateless(size_t size);

    template <uint32_t commandType>
    LinearStream *obtainCommandStream(const CsrDependencies &csrDependencies, bool blitEnqueue, bool blockedQueue,
                                      const MultiDispatchInfo &multiDispatchInfo, const EventsRequest &eventsRequest,
                                      std::unique_ptr<KernelOperation> &blockedCommandsData,
                                      Surface **surfaces, size_t numSurfaces, bool isMarkerWithProfiling, bool resolveDependenciesByPipecontrol) {
        LinearStream *commandStream = nullptr;

        bool profilingRequired = (this->isProfilingEnabled() && eventsRequest.outEvent);
        bool perfCountersRequired = (this->isPerfCountersEnabled() && eventsRequest.outEvent);

        if (isBlockedCommandStreamRequired(commandType, eventsRequest, blockedQueue, isMarkerWithProfiling)) {
            constexpr size_t additionalAllocationSize = CSRequirements::csOverfetchSize;
            constexpr size_t allocationSize = MemoryConstants::pageSize64k - CSRequirements::csOverfetchSize;
            commandStream = new LinearStream();

            auto &gpgpuCsr = getGpgpuCommandStreamReceiver();
            gpgpuCsr.ensureCommandBufferAllocation(*commandStream, allocationSize, additionalAllocationSize);

            blockedCommandsData = std::make_unique<KernelOperation>(commandStream, *gpgpuCsr.getInternalAllocationStorage());
        } else {
            commandStream = &getCommandStream<GfxFamily, commandType>(*this, csrDependencies, profilingRequired, perfCountersRequired,
                                                                      blitEnqueue, multiDispatchInfo, surfaces, numSurfaces, isMarkerWithProfiling, eventsRequest.numEventsInWaitList > 0, resolveDependenciesByPipecontrol, eventsRequest.outEvent);
        }
        return commandStream;
    }

    void processDispatchForBlitAuxTranslation(CommandStreamReceiver &bcsCsr, const MultiDispatchInfo &multiDispatchInfo, BlitPropertiesContainer &blitPropertiesContainer,
                                              TimestampPacketDependencies &timestampPacketDependencies, const EventsRequest &eventsRequest,
                                              bool queueBlocked);

    bool isTaskLevelUpdateRequired(const TaskCountType &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType);
    void obtainTaskLevelAndBlockedStatus(TaskCountType &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueueStatus, unsigned int commandType) override;
    static void computeOffsetsValueForRectCommands(size_t *bufferOffset,
                                                   size_t *hostOffset,
                                                   const size_t *bufferOrigin,
                                                   const size_t *hostOrigin,
                                                   const size_t *region,
                                                   size_t bufferRowPitch,
                                                   size_t bufferSlicePitch,
                                                   size_t hostRowPitch,
                                                   size_t hostSlicePitch);

    template <uint32_t commandType>
    void processDispatchForKernels(const MultiDispatchInfo &multiDispatchInfo,
                                   std::unique_ptr<PrintfHandler> &printfHandler,
                                   Event *event,
                                   TagNodeBase *&hwTimeStamps,
                                   bool blockQueue,
                                   CsrDependencies &csrDeps,
                                   KernelOperation *blockedCommandsData,
                                   TimestampPacketDependencies &timestampPacketDependencies,
                                   bool relaxedOrderingEnabled,
                                   bool blocking);

    MOCKABLE_VIRTUAL bool isGpgpuSubmissionForBcsRequired(bool queueBlocked, TimestampPacketDependencies &timestampPacketDependencies, bool containsCrossEngineDependency, bool textureCacheFlushRequired) const;
    void setupEvent(EventBuilder &eventBuilder, cl_event *outEvent, uint32_t cmdType);

    bool isBlitAuxTranslationRequired(const MultiDispatchInfo &multiDispatchInfo);
    bool relaxedOrderingForGpgpuAllowed(uint32_t numWaitEvents) const;
};
} // namespace NEO

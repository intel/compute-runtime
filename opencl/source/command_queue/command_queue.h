/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/map_operation_type.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/sku_info/sku_info_base.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/range.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/command_queue/copy_engine_state.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/enqueue_properties.h"
#include "opencl/source/helpers/properties_helper.h"

#include <cstdint>
#include <optional>

namespace NEO {
class BarrierCommand;
class Buffer;
class ClDevice;
class Context;
class Event;
class EventBuilder;
class FlushStampTracker;
class Image;
class IndirectHeap;
class Kernel;
class LinearStream;
class PerformanceCounters;
class PrintfHandler;
enum class WaitStatus;
struct BuiltinOpParams;
struct CsrSelectionArgs;
struct MultiDispatchInfo;
struct TimestampPacketDependencies;
struct StagingTransferStatus;

enum class QueuePriority {
    low,
    medium,
    high
};

template <>
struct OpenCLObjectMapper<_cl_command_queue> {
    typedef class CommandQueue DerivedType;
};

class CommandQueue : public BaseObject<_cl_command_queue> {
  public:
    static const cl_ulong objectMagic = 0x1234567890987654LL;

    static CommandQueue *create(Context *context,
                                ClDevice *device,
                                const cl_queue_properties *properties,
                                bool internalUsage,
                                cl_int &errcodeRet);

    static cl_int getErrorCodeFromTaskCount(TaskCountType taskCount);

    CommandQueue() = delete;

    CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties, bool internalUsage);

    ~CommandQueue() override;

    // API entry points
    virtual cl_int enqueueCopyImage(Image *srcImage, Image *dstImage, const size_t *srcOrigin, const size_t *dstOrigin,
                                    const size_t *region, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueFillImage(Image *image, const void *fillColor, const size_t *origin, const size_t *region,
                                    cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueFillBuffer(Buffer *buffer, const void *pattern, size_t patternSize, size_t offset,
                                     size_t size, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueKernel(Kernel *kernel, cl_uint workDim, const size_t *globalWorkOffset, const size_t *globalWorkSize,
                                 const size_t *localWorkSize, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueBarrierWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

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

    virtual cl_int enqueueSVMMap(cl_bool blockingMap, cl_map_flags mapFlags, void *svmPtr, size_t size,
                                 cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool externalAppCall) = 0;

    virtual cl_int enqueueSVMUnmap(void *svmPtr, cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                   cl_event *event, bool externalAppCall) = 0;

    virtual cl_int enqueueSVMFree(cl_uint numSvmPointers, void *svmPointers[],
                                  void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                                 cl_uint numSvmPointers,
                                                                 void *svmPointers[],
                                                                 void *userData),
                                  void *userData, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueSVMMemcpy(cl_bool blockingCopy, void *dstPtr, const void *srcPtr, size_t size, cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver *csrParam) = 0;

    virtual cl_int enqueueSVMMemFill(void *svmPtr, const void *pattern, size_t patternSize,
                                     size_t size, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueMarkerWithWaitList(cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueMigrateMemObjects(cl_uint numMemObjects, const cl_mem *memObjects, cl_mem_migration_flags flags,
                                            cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueSVMMigrateMem(cl_uint numSvmPointers, const void **svmPointers, const size_t *sizes,
                                        const cl_mem_migration_flags flags, cl_uint numEventsInWaitList,
                                        const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueCopyBuffer(Buffer *srcBuffer, Buffer *dstBuffer, size_t srcOffset, size_t dstOffset,
                                     size_t size, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueReadBuffer(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t size, void *ptr,
                                     GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList, cl_event *event) = 0;
    virtual cl_int enqueueReadBufferImpl(Buffer *buffer, cl_bool blockingRead, size_t offset, size_t size,
                                         void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) = 0;

    virtual cl_int enqueueReadImage(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region,
                                    size_t rowPitch, size_t slicePitch, void *ptr, GraphicsAllocation *mapAllocation,
                                    cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueReadImageImpl(Image *srcImage, cl_bool blockingRead, const size_t *origin, const size_t *region,
                                        size_t rowPitch, size_t slicePitch, void *ptr, GraphicsAllocation *mapAllocation,
                                        cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) = 0;

    virtual cl_int enqueueWriteBuffer(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t cb,
                                      const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueWriteImage(Image *dstImage, cl_bool blockingWrite, const size_t *origin,
                                     const size_t *region, size_t inputRowPitch, size_t inputSlicePitch,
                                     const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueWriteBufferImpl(Buffer *buffer, cl_bool blockingWrite, size_t offset, size_t cb,
                                          const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) = 0;

    virtual cl_int enqueueWriteImageImpl(Image *dstImage, cl_bool blockingWrite, const size_t *origin,
                                         const size_t *region, size_t inputRowPitch, size_t inputSlicePitch,
                                         const void *ptr, GraphicsAllocation *mapAllocation, cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList, cl_event *event, CommandStreamReceiver &csr) = 0;

    virtual cl_int enqueueCopyBufferRect(Buffer *srcBuffer, Buffer *dstBuffer, const size_t *srcOrigin, const size_t *dstOrigin,
                                         const size_t *region, size_t srcRowPitch, size_t srcSlicePitch, size_t dstRowPitch, size_t dstSlicePitch,
                                         cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueWriteBufferRect(Buffer *buffer, cl_bool blockingWrite, const size_t *bufferOrigin,
                                          const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
                                          size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
                                          const void *ptr, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueReadBufferRect(Buffer *buffer, cl_bool blockingRead, const size_t *bufferOrigin,
                                         const size_t *hostOrigin, const size_t *region, size_t bufferRowPitch,
                                         size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
                                         void *ptr, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueCopyBufferToImage(Buffer *srcBuffer, Image *dstImage, size_t srcOffset,
                                            const size_t *dstOrigin, const size_t *region,
                                            cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int enqueueCopyImageToBuffer(Image *srcImage, Buffer *dstBuffer, const size_t *srcOrigin, const size_t *region,
                                            size_t dstOffset, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) = 0;

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

    virtual cl_int enqueueResourceBarrier(BarrierCommand *resourceBarrier, cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList, cl_event *event) = 0;

    virtual cl_int finish() = 0;

    virtual cl_int flush() = 0;

    void updateFromCompletionStamp(const CompletionStamp &completionStamp, Event *outEvent);

    virtual bool isCacheFlushCommand(uint32_t commandType) const { return false; }

    cl_int getCommandQueueInfo(cl_command_queue_info paramName,
                               size_t paramValueSize, void *paramValue,
                               size_t *paramValueSizeRet);

    TagAddressType getHwTag() const;

    volatile TagAddressType *getHwTagAddress() const;

    MOCKABLE_VIRTUAL bool isCompleted(TaskCountType gpgpuTaskCount, const Range<CopyEngineState> &bcsStates);

    bool isWaitForTimestampsEnabled() const;
    virtual bool waitForTimestamps(Range<CopyEngineState> copyEnginesToWait, WaitStatus &status, TimestampPacketContainer *mainContainer, TimestampPacketContainer *deferredContainer) = 0;

    MOCKABLE_VIRTUAL bool isQueueBlocked();

    MOCKABLE_VIRTUAL WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait);
    MOCKABLE_VIRTUAL WaitStatus waitUntilComplete(TaskCountType gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) {
        return this->waitUntilComplete(gpgpuTaskCountToWait, copyEnginesToWait, flushStampToWait, useQuickKmdSleep, true, false);
    }
    MOCKABLE_VIRTUAL WaitStatus waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList);
    MOCKABLE_VIRTUAL WaitStatus waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler) {
        return this->waitForAllEngines(blockedQueue, printfHandler, true);
    }

    static TaskCountType getTaskLevelFromWaitList(TaskCountType taskLevel,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *eventWaitList);

    void initializeGpgpu() const;
    void initializeGpgpuInternals() const;
    MOCKABLE_VIRTUAL CommandStreamReceiver &getGpgpuCommandStreamReceiver() const;
    MOCKABLE_VIRTUAL CommandStreamReceiver *getBcsCommandStreamReceiver(aub_stream::EngineType bcsEngineType);
    CommandStreamReceiver *getBcsForAuxTranslation();
    MOCKABLE_VIRTUAL CommandStreamReceiver &selectCsrForBuiltinOperation(const CsrSelectionArgs &args);
    void constructBcsEngine(bool internalUsage);
    MOCKABLE_VIRTUAL void initializeBcsEngine(bool internalUsage);
    void constructBcsEnginesForSplit();
    void prepareHostPtrSurfaceForSplit(bool split, GraphicsAllocation &allocation);
    CommandStreamReceiver &selectCsrForHostPtrAllocation(bool split, CommandStreamReceiver &csr);
    void releaseMainCopyEngine();
    Device &getDevice() const noexcept;
    ClDevice &getClDevice() const { return *device; }
    Context &getContext() const { return *context; }
    Context *getContextPtr() const { return context; }
    EngineControl &getGpgpuEngine() const {
        this->initializeGpgpu();
        return *gpgpuEngine;
    }

    MOCKABLE_VIRTUAL LinearStream &getCS(size_t minRequiredSize);
    IndirectHeap &getIndirectHeap(IndirectHeapType heapType,
                                  size_t minRequiredSize);

    void allocateHeapMemory(IndirectHeapType heapType,
                            size_t minRequiredSize, IndirectHeap *&indirectHeap);

    static bool isTimestampWaitEnabled();

    MOCKABLE_VIRTUAL void releaseIndirectHeap(IndirectHeapType heapType);

    void releaseVirtualEvent();

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

    bool setPerfCountersEnabled();

    void setIsSpecialCommandQueue(bool newValue) {
        this->isSpecialCommandQueue = newValue;
    }

    bool isSpecial() {
        return this->isSpecialCommandQueue;
    }

    QueuePriority getPriority() const {
        return priority;
    }

    QueueThrottle getThrottle() const {
        return throttle;
    }

    const TimestampPacketContainer *getTimestampPacketContainer() const {
        return timestampPacketContainer.get();
    }

    const std::vector<uint64_t> &getPropertiesVector() const { return propertiesVector; }

    void enqueueBlockedMapUnmapOperation(const cl_event *eventWaitList,
                                         size_t numEventsInWaitlist,
                                         MapOperationType opType,
                                         MemObj *memObj,
                                         MemObjSizeArray &copySize,
                                         MemObjOffsetArray &copyOffset,
                                         bool readOnly,
                                         EventBuilder &externalEventBuilder);

    MOCKABLE_VIRTUAL bool setupDebugSurface(Kernel *kernel);

    bool validateCapability(cl_command_queue_capabilities_intel capability) const;
    bool validateCapabilitiesForEventWaitList(cl_uint numEventsInWaitList, const cl_event *waitList) const;
    bool validateCapabilityForOperation(cl_command_queue_capabilities_intel capability, cl_uint numEventsInWaitList, const cl_event *waitList, const cl_event *outEvent) const;
    cl_uint getQueueFamilyIndex() const;
    cl_uint getQueueIndexWithinFamily() const { return queueIndexWithinFamily; }
    bool isQueueFamilySelected() const { return queueFamilySelected; }

    bool getRequiresCacheFlushAfterWalker() const {
        return requiresCacheFlushAfterWalker;
    }

    template <typename PtrType>
    static PtrType convertAddressWithOffsetToGpuVa(PtrType ptr, InternalMemoryType memoryType, GraphicsAllocation &allocation);

    void updateBcsTaskCount(aub_stream::EngineType bcsEngineType, TaskCountType newBcsTaskCount);
    TaskCountType peekBcsTaskCount(aub_stream::EngineType bcsEngineType) const;

    void updateLatestSentEnqueueType(EnqueueProperties::Operation newEnqueueType) { this->latestSentEnqueueType = newEnqueueType; }
    EnqueueProperties::Operation peekLatestSentEnqueueOperation() { return this->latestSentEnqueueType; }

    void setupBarrierTimestampForBcsEngines(aub_stream::EngineType engineType, TimestampPacketDependencies &timestampPacketDependencies);
    void processBarrierTimestampForBcsEngine(aub_stream::EngineType bcsEngineType, TimestampPacketDependencies &blitDependencies);
    void setLastBcsPacket(aub_stream::EngineType bcsEngineType);
    void fillCsrDependenciesWithLastBcsPackets(CsrDependencies &csrDeps);
    void clearLastBcsPackets();

    void setStallingCommandsOnNextFlush(const bool isStallingCommandsOnNextFlushRequired) {
        stallingCommandsOnNextFlushRequired = isStallingCommandsOnNextFlushRequired;
        if (!isStallingCommandsOnNextFlushRequired) {
            dcFlushRequiredOnStallingCommandsOnNextFlush = false;
        }
    }
    bool isStallingCommandsOnNextFlushRequired() const { return stallingCommandsOnNextFlushRequired; }

    void setDcFlushRequiredOnStallingCommandsOnNextFlush(const bool isDcFlushRequiredOnStallingCommandsOnNextFlush) { dcFlushRequiredOnStallingCommandsOnNextFlush = isDcFlushRequiredOnStallingCommandsOnNextFlush; }
    bool isDcFlushRequiredOnStallingCommandsOnNextFlush() const { return dcFlushRequiredOnStallingCommandsOnNextFlush; }

    // taskCount of last task
    TaskCountType taskCount = 0;

    // current taskLevel. Used for determining if a PIPE_CONTROL is needed.
    TaskCountType taskLevel = 0;

    std::unique_ptr<FlushStampTracker> flushStamp;

    // virtual event that holds last Enqueue information
    Event *virtualEvent = nullptr;

    size_t estimateTimestampPacketNodesCount(const MultiDispatchInfo &dispatchInfo) const;

    uint64_t getSliceCount() const { return sliceCount; }

    TimestampPacketContainer *getDeferredTimestampPackets() const { return deferredTimestampPackets.get(); }

    uint64_t dispatchHints = 0;

    bool isTextureCacheFlushNeeded(uint32_t commandType) const;

    const std::array<CopyEngineState, bcsInfoMaskSize> &peekActiveBcsStates() const { return bcsStates; }

    void handlePostCompletionOperations(bool checkQueueCompletion);

    bool getHeaplessModeEnabled() const { return this->heaplessModeEnabled; }
    bool getHeaplessStateInitEnabled() const { return this->heaplessStateInitEnabled; }

    bool isBcsSplitInitialized() const { return this->bcsSplitInitialized; }
    bool isBcs() const { return isCopyOnly; };

    cl_int enqueueStagingBufferMemcpy(cl_bool blockingCopy, void *dstPtr, const void *srcPtr, size_t size, cl_event *event);
    cl_int enqueueStagingImageTransfer(cl_command_type commandType, Image *dstImage, cl_bool blockingCopy, const size_t *globalOrigin, const size_t *globalRegion,
                                       size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, cl_event *event);
    cl_int enqueueStagingBufferTransfer(cl_command_type commandType, Buffer *buffer, cl_bool blockingCopy, size_t offset, size_t size, const void *ptr, cl_event *event);

    bool isValidForStagingBufferCopy(Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies);
    bool isValidForStagingTransfer(MemObj *memObj, const void *ptr, size_t size, cl_command_type commandType, bool isBlocking, bool hasDependencies);

    size_t calculateHostPtrSizeForImage(const size_t *region, size_t rowPitch, size_t slicePitch, Image *image) const;

    bool isCacheFlushForImageRequired(cl_int cmdType) const {
        return this->isCacheFlushOnNextBcsWriteRequired && this->isImageWriteOperation(cmdType);
    }

  protected:
    void *enqueueReadMemObjForMap(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest);

    void *enqueueMapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet);
    cl_int enqueueUnmapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest);

    virtual void obtainTaskLevelAndBlockedStatus(TaskCountType &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueueStatus, unsigned int commandType){};
    bool isBlockedCommandStreamRequired(uint32_t commandType, const EventsRequest &eventsRequest, bool blockedQueue, bool isMarkerWithProfiling) const;

    MOCKABLE_VIRTUAL void obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies, CommandStreamReceiver &csr);
    void storeProperties(const cl_queue_properties *properties);
    void processProperties(const cl_queue_properties *properties);
    void overrideEngine(aub_stream::EngineType engineType, EngineUsage engineUsage);
    bool bufferCpuCopyAllowed(Buffer *buffer, cl_command_type commandType, cl_bool blocking, size_t size, void *ptr,
                              cl_uint numEventsInWaitList, const cl_event *eventWaitList);
    void providePerformanceHint(TransferProperties &transferProperties);
    bool queueDependenciesClearRequired() const;
    bool blitEnqueueAllowed(const CsrSelectionArgs &args) const;
    MOCKABLE_VIRTUAL bool migrateMultiGraphicsAllocationsIfRequired(const BuiltinOpParams &operationParams, CommandStreamReceiver &csr);

    inline bool shouldFlushDC(uint32_t commandType, PrintfHandler *printfHandler) const {
        return (commandType == CL_COMMAND_READ_BUFFER ||
                commandType == CL_COMMAND_READ_BUFFER_RECT ||
                commandType == CL_COMMAND_READ_IMAGE ||
                commandType == CL_COMMAND_SVM_MAP ||
                printfHandler ||
                isTextureCacheFlushNeeded(commandType));
    }

    bool isImageWriteOperation(cl_command_type commandType) const {
        switch (commandType) {
        case CL_COMMAND_WRITE_IMAGE:
        case CL_COMMAND_COPY_IMAGE:
        case CL_COMMAND_FILL_IMAGE:
        case CL_COMMAND_COPY_BUFFER_TO_IMAGE:
            return true;
        default:
            return false;
        }
    }

    MOCKABLE_VIRTUAL bool blitEnqueueImageAllowed(const size_t *origin, const size_t *region, const Image &image) const;
    void aubCaptureHook(bool &blocking, bool &clearAllDependencies, const MultiDispatchInfo &multiDispatchInfo);
    void assignDataToOverwrittenBcsNode(TagNodeBase *node);

    void registerGpgpuCsrClient();
    void registerBcsCsrClient(CommandStreamReceiver &bcsCsr);

    void unregisterGpgpuCsrClient();
    void unregisterBcsCsrClient(CommandStreamReceiver &bcsCsr);

    void unregisterGpgpuAndBcsCsrClients();

    cl_int postStagingTransferSync(const StagingTransferStatus &status, cl_event *event, const cl_event profilingEvent, bool isSingleTransfer, bool isBlocking, cl_command_type commandType);
    cl_event *assignEventForStaging(cl_event *userEvent, cl_event *profilingEvent, bool isFirstTransfer, bool isLastTransfer) const;

    Context *context = nullptr;
    ClDevice *device = nullptr;
    mutable EngineControl *gpgpuEngine = nullptr;
    std::array<EngineControl *, bcsInfoMaskSize> bcsEngines = {};
    std::optional<aub_stream::EngineType> bcsQueueEngineType{};
    size_t bcsEngineCount = bcsInfoMaskSize;

    cl_command_queue_properties commandQueueProperties = 0;
    std::vector<uint64_t> propertiesVector;

    cl_command_queue_capabilities_intel queueCapabilities = CL_QUEUE_DEFAULT_CAPABILITIES_INTEL;
    cl_uint queueFamilyIndex = 0;
    cl_uint queueIndexWithinFamily = 0;
    bool queueFamilySelected = false;

    QueuePriority priority = QueuePriority::medium;
    QueueThrottle throttle = QueueThrottle::MEDIUM;
    EnqueueProperties::Operation latestSentEnqueueType = EnqueueProperties::Operation::none;
    uint64_t sliceCount = QueueSliceCount::defaultSliceCount;
    std::array<CopyEngineState, bcsInfoMaskSize> bcsStates = {};

    bool perfCountersEnabled = false;
    bool isInternalUsage = false;
    bool isCopyOnly = false;
    bool bcsAllowed = false;
    bool bcsInitialized = false;

    bool bcsSplitInitialized = false;
    BcsInfoMask splitEngines = EngineHelpers::oddLinkedCopyEnginesMask;
    BcsInfoMask h2dEngines = NEO::EngineHelpers::h2dCopyEngineMask;
    BcsInfoMask d2hEngines = NEO::EngineHelpers::d2hCopyEngineMask;
    size_t minimalSizeForBcsSplit = 16 * MemoryConstants::megaByte;

    LinearStream *commandStream = nullptr;

    bool isSpecialCommandQueue = false;
    bool requiresCacheFlushAfterWalker = false;

    std::unique_ptr<TimestampPacketContainer> deferredTimestampPackets;
    std::unique_ptr<TimestampPacketContainer> deferredMultiRootSyncNodes;
    std::unique_ptr<TimestampPacketContainer> timestampPacketContainer;

    struct BcsTimestampPacketContainers {
        TimestampPacketContainer lastBarrierToWaitFor;
        TimestampPacketContainer lastSignalledPacket;
    };
    std::array<BcsTimestampPacketContainers, bcsInfoMaskSize> bcsTimestampPacketContainers;
    bool stallingCommandsOnNextFlushRequired = false;
    bool dcFlushRequiredOnStallingCommandsOnNextFlush = false;
    bool isCacheFlushOnNextBcsWriteRequired = false;
    bool splitBarrierRequired = false;
    bool gpgpuCsrClientRegistered = false;
    bool heaplessModeEnabled = false;
    bool heaplessStateInitEnabled = false;
    bool isForceStateless = false;
    bool l3FlushedAfterCpuRead = true;
    bool l3FlushAfterPostSyncEnabled = false;
};

static_assert(NEO::NonCopyableAndNonMovable<CommandQueue>);

template <typename PtrType>
PtrType CommandQueue::convertAddressWithOffsetToGpuVa(PtrType ptr, InternalMemoryType memoryType, GraphicsAllocation &allocation) {
    // If this is device or shared USM pointer, it is already a gpuVA and we don't have to do anything.
    // Otherwise, we assume this is a cpuVA and we have to convert to gpuVA, while preserving offset from allocation start.
    const bool isCpuPtr = (memoryType != InternalMemoryType::deviceUnifiedMemory) && (memoryType != InternalMemoryType::sharedUnifiedMemory);
    if (isCpuPtr) {
        size_t dstOffset = ptrDiff(ptr, allocation.getUnderlyingBuffer());
        ptr = reinterpret_cast<PtrType>(allocation.getGpuAddress() + dstOffset);
    }
    return ptr;
}

using CommandQueueCreateFunc = CommandQueue *(*)(Context *context, ClDevice *device, const cl_queue_properties *properties, bool internalUsage);

} // namespace NEO

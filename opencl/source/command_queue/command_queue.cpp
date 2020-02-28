/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/context/context.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"

#include "CL/cl_ext.h"

#include <map>

namespace NEO {

// Global table of create functions
CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE] = {};

CommandQueue *CommandQueue::create(Context *context,
                                   ClDevice *device,
                                   const cl_queue_properties *properties,
                                   bool internalUsage,
                                   cl_int &retVal) {
    retVal = CL_SUCCESS;

    auto funcCreate = commandQueueFactory[device->getRenderCoreFamily()];
    DEBUG_BREAK_IF(nullptr == funcCreate);

    return funcCreate(context, device, properties, internalUsage);
}

CommandQueue::CommandQueue() : CommandQueue(nullptr, nullptr, 0) {
}

CommandQueue::CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties)
    : context(context), device(device) {
    if (context) {
        context->incRefInternal();
    }

    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    flushStamp.reset(new FlushStampTracker(true));

    if (device) {
        gpgpuEngine = &device->getDefaultEngine();
        if (gpgpuEngine->commandStreamReceiver->peekTimestampPacketWriteEnabled()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
        }
        auto hwInfo = device->getHardwareInfo();
        if (hwInfo.capabilityTable.blitterOperationsSupported) {
            auto &selectorCopyEngine = device->getDeviceById(0)->getSelectorCopyEngine();
            bcsEngine = &device->getDeviceById(0)->getEngine(EngineHelpers::getBcsEngineType(hwInfo, selectorCopyEngine), false);
        }
    }

    processProperties(properties);
}

CommandQueue::~CommandQueue() {
    if (virtualEvent) {
        UNRECOVERABLE_IF(this->virtualEvent->getCommandQueue() != this && this->virtualEvent->getCommandQueue() != nullptr);
        virtualEvent->decRefInternal();
    }

    if (device) {
        auto storageForAllocation = gpgpuEngine->commandStreamReceiver->getInternalAllocationStorage();

        if (commandStream) {
            storageForAllocation->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream->getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }
        delete commandStream;

        if (this->perfCountersEnabled) {
            device->getPerformanceCounters()->shutdown();
        }
    }

    timestampPacketContainer.reset();
    //for normal queue, decrement ref count on context
    //special queue is owned by context so ref count doesn't have to be decremented
    if (context && !isSpecialCommandQueue) {
        context->decRefInternal();
    }
}

CommandStreamReceiver &CommandQueue::getGpgpuCommandStreamReceiver() const {
    return *gpgpuEngine->commandStreamReceiver;
}

CommandStreamReceiver *CommandQueue::getBcsCommandStreamReceiver() const {
    if (bcsEngine) {
        return bcsEngine->commandStreamReceiver;
    }
    return nullptr;
}

Device &CommandQueue::getDevice() const noexcept {
    return device->getDevice();
}

uint32_t CommandQueue::getHwTag() const {
    uint32_t tag = *getHwTagAddress();
    return tag;
}

volatile uint32_t *CommandQueue::getHwTagAddress() const {
    return getGpgpuCommandStreamReceiver().getTagAddress();
}

bool CommandQueue::isCompleted(uint32_t taskCount) const {
    uint32_t tag = getHwTag();
    DEBUG_BREAK_IF(tag == CompletionStamp::levelNotReady);
    return tag >= taskCount;
}

void CommandQueue::waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) {
    WAIT_ENTER()

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Waiting for taskCount:", taskCountToWait);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", getHwTag());

    bool forcePowerSavingMode = this->throttle == QueueThrottle::LOW;

    getGpgpuCommandStreamReceiver().waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait,
                                                                          useQuickKmdSleep, forcePowerSavingMode);
    DEBUG_BREAK_IF(getHwTag() < taskCountToWait);

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyTaskCompletion(taskCountToWait);
    }

    if (auto bcsCsr = getBcsCommandStreamReceiver()) {
        bcsCsr->waitForTaskCountWithKmdNotifyFallback(bcsTaskCount, 0, false, false);
        bcsCsr->waitForTaskCountAndCleanTemporaryAllocationList(bcsTaskCount);
    }

    getGpgpuCommandStreamReceiver().waitForTaskCountAndCleanTemporaryAllocationList(taskCountToWait);

    WAIT_LEAVE()
}

bool CommandQueue::isQueueBlocked() {
    TakeOwnershipWrapper<CommandQueue> takeOwnershipWrapper(*this);
    //check if we have user event and if so, if it is in blocked state.
    if (this->virtualEvent) {
        auto executionStatus = this->virtualEvent->peekExecutionStatus();
        if (executionStatus <= CL_SUBMITTED) {
            UNRECOVERABLE_IF(this->virtualEvent == nullptr);

            if (this->virtualEvent->isStatusCompletedByTermination(executionStatus) == false) {
                taskCount = this->virtualEvent->peekTaskCount();
                flushStamp->setStamp(this->virtualEvent->flushStamp->peekStamp());
                taskLevel = this->virtualEvent->taskLevel;
                // If this isn't an OOQ, update the taskLevel for the queue
                if (!isOOQEnabled()) {
                    taskLevel++;
                }
            } else {
                //at this point we may reset queue TaskCount, since all command previous to this were aborted
                taskCount = 0;
                flushStamp->setStamp(0);
                taskLevel = getGpgpuCommandStreamReceiver().peekTaskLevel();
            }

            FileLoggerInstance().log(DebugManager.flags.EventsDebugEnable.get(), "isQueueBlocked taskLevel change from", taskLevel, "to new from virtualEvent", this->virtualEvent, "new tasklevel", this->virtualEvent->taskLevel.load());

            //close the access to virtual event, driver added only 1 ref count.
            this->virtualEvent->decRefInternal();
            this->virtualEvent = nullptr;
            return false;
        }
        return true;
    }
    return false;
}

cl_int CommandQueue::getCommandQueueInfo(cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    return getQueueInfo<CommandQueue>(this, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

uint32_t CommandQueue::getTaskLevelFromWaitList(uint32_t taskLevel,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList) {
    for (auto iEvent = 0u; iEvent < numEventsInWaitList; ++iEvent) {
        auto pEvent = (Event *)(eventWaitList[iEvent]);
        uint32_t eventTaskLevel = pEvent->taskLevel;
        taskLevel = std::max(taskLevel, eventTaskLevel);
    }
    return taskLevel;
}

LinearStream &CommandQueue::getCS(size_t minRequiredSize) {
    DEBUG_BREAK_IF(nullptr == device);

    if (!commandStream) {
        commandStream = new LinearStream(nullptr);
    }

    minRequiredSize += CSRequirements::minCommandQueueCommandStreamSize;
    constexpr static auto additionalAllocationSize = CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize;
    getGpgpuCommandStreamReceiver().ensureCommandBufferAllocation(*commandStream, minRequiredSize, additionalAllocationSize);
    return *commandStream;
}

cl_int CommandQueue::enqueueAcquireSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        int result = memObject->peekSharingHandler()->acquire(memObject);
        if (result != CL_SUCCESS) {
            return result;
        }
        memObject->acquireCount++;
    }
    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }

    return status;
}

cl_int CommandQueue::enqueueReleaseSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        memObject->peekSharingHandler()->release(memObject);
        DEBUG_BREAK_IF(memObject->acquireCount <= 0);
        memObject->acquireCount--;
    }
    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }
    return status;
}

void CommandQueue::updateFromCompletionStamp(const CompletionStamp &completionStamp) {
    DEBUG_BREAK_IF(this->taskLevel > completionStamp.taskLevel);
    DEBUG_BREAK_IF(this->taskCount > completionStamp.taskCount);
    if (completionStamp.taskCount != CompletionStamp::levelNotReady) {
        taskCount = completionStamp.taskCount;
    }
    flushStamp->setStamp(completionStamp.flushStamp);
    this->taskLevel = completionStamp.taskLevel;
}

bool CommandQueue::setPerfCountersEnabled() {
    DEBUG_BREAK_IF(device == nullptr);

    auto perfCounters = device->getPerformanceCounters();
    bool isCcsEngine = EngineHelpers::isCcs(getGpgpuEngine().osContext->getEngineType());

    perfCountersEnabled = perfCounters->enable(isCcsEngine);

    if (!perfCountersEnabled) {
        perfCounters->shutdown();
    }

    return perfCountersEnabled;
}

PerformanceCounters *CommandQueue::getPerfCounters() {
    return device->getPerformanceCounters();
}

cl_int CommandQueue::enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest) {
    cl_int retVal = CL_SUCCESS;

    MapInfo unmapInfo;
    if (!memObj->findMappedPtr(mappedPtr, unmapInfo)) {
        return CL_INVALID_VALUE;
    }

    if (!unmapInfo.readOnly) {
        memObj->getMapAllocation()->setAubWritable(true, GraphicsAllocation::defaultBank);
        memObj->getMapAllocation()->setTbxWritable(true, GraphicsAllocation::defaultBank);

        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            auto buffer = castToObject<Buffer>(memObj);

            retVal = enqueueWriteBuffer(buffer, CL_TRUE, unmapInfo.offset[0], unmapInfo.size[0], mappedPtr, memObj->getMapAllocation(),
                                        eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
        } else {
            auto image = castToObjectOrAbort<Image>(memObj);
            size_t writeOrigin[4] = {unmapInfo.offset[0], unmapInfo.offset[1], unmapInfo.offset[2], 0};
            auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
            UNRECOVERABLE_IF(mipIdx >= 4);
            writeOrigin[mipIdx] = unmapInfo.mipLevel;
            retVal = enqueueWriteImage(image, CL_FALSE, writeOrigin, &unmapInfo.size[0],
                                       image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(), mappedPtr, memObj->getMapAllocation(),
                                       eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
        }
    } else {
        retVal = enqueueMarkerWithWaitList(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
    }

    if (retVal == CL_SUCCESS) {
        memObj->removeMappedPtr(mappedPtr);
        if (eventsRequest.outEvent) {
            auto event = castToObject<Event>(*eventsRequest.outEvent);
            event->setCmdType(CL_COMMAND_UNMAP_MEM_OBJECT);
        }
    }
    return retVal;
}

void *CommandQueue::enqueueReadMemObjForMap(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet) {
    void *basePtr = transferProperties.memObj->getBasePtrForMap(getDevice().getRootDeviceIndex());
    size_t mapPtrOffset = transferProperties.memObj->calculateOffsetForMapping(transferProperties.offset) + transferProperties.mipPtrOffset;
    if (transferProperties.memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
        mapPtrOffset += transferProperties.memObj->getOffset();
    }
    void *returnPtr = ptrOffset(basePtr, mapPtrOffset);

    if (!transferProperties.memObj->addMappedPtr(returnPtr, transferProperties.memObj->calculateMappedPtrLength(transferProperties.size),
                                                 transferProperties.mapFlags, transferProperties.size, transferProperties.offset, transferProperties.mipLevel)) {
        errcodeRet = CL_INVALID_OPERATION;
        return nullptr;
    }

    if (transferProperties.memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
        auto buffer = castToObject<Buffer>(transferProperties.memObj);
        errcodeRet = enqueueReadBuffer(buffer, transferProperties.blocking, transferProperties.offset[0], transferProperties.size[0],
                                       returnPtr, transferProperties.memObj->getMapAllocation(), eventsRequest.numEventsInWaitList,
                                       eventsRequest.eventWaitList, eventsRequest.outEvent);
    } else {
        auto image = castToObjectOrAbort<Image>(transferProperties.memObj);
        size_t readOrigin[4] = {transferProperties.offset[0], transferProperties.offset[1], transferProperties.offset[2], 0};
        auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
        UNRECOVERABLE_IF(mipIdx >= 4);
        readOrigin[mipIdx] = transferProperties.mipLevel;
        errcodeRet = enqueueReadImage(image, transferProperties.blocking, readOrigin, &transferProperties.size[0],
                                      image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(),
                                      returnPtr, transferProperties.memObj->getMapAllocation(), eventsRequest.numEventsInWaitList,
                                      eventsRequest.eventWaitList, eventsRequest.outEvent);
    }

    if (errcodeRet != CL_SUCCESS) {
        transferProperties.memObj->removeMappedPtr(returnPtr);
        return nullptr;
    }
    if (eventsRequest.outEvent) {
        auto event = castToObject<Event>(*eventsRequest.outEvent);
        event->setCmdType(transferProperties.cmdType);
    }
    return returnPtr;
}

void *CommandQueue::enqueueMapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet) {
    if (transferProperties.memObj->mappingOnCpuAllowed()) {
        return cpuDataTransferHandler(transferProperties, eventsRequest, errcodeRet);
    } else {
        return enqueueReadMemObjForMap(transferProperties, eventsRequest, errcodeRet);
    }
}

cl_int CommandQueue::enqueueUnmapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest) {
    cl_int retVal = CL_SUCCESS;
    if (transferProperties.memObj->mappingOnCpuAllowed()) {
        cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    } else {
        retVal = enqueueWriteMemObjForUnmap(transferProperties.memObj, transferProperties.ptr, eventsRequest);
    }
    return retVal;
}

void *CommandQueue::enqueueMapBuffer(Buffer *buffer, cl_bool blockingMap,
                                     cl_map_flags mapFlags, size_t offset,
                                     size_t size, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList, cl_event *event,
                                     cl_int &errcodeRet) {
    TransferProperties transferProperties(buffer, CL_COMMAND_MAP_BUFFER, mapFlags, blockingMap != CL_FALSE, &offset, &size, nullptr, false);
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    return enqueueMapMemObject(transferProperties, eventsRequest, errcodeRet);
}

void *CommandQueue::enqueueMapImage(Image *image, cl_bool blockingMap,
                                    cl_map_flags mapFlags, const size_t *origin,
                                    const size_t *region, size_t *imageRowPitch,
                                    size_t *imageSlicePitch,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList, cl_event *event,
                                    cl_int &errcodeRet) {
    TransferProperties transferProperties(image, CL_COMMAND_MAP_IMAGE, mapFlags, blockingMap != CL_FALSE,
                                          const_cast<size_t *>(origin), const_cast<size_t *>(region), nullptr, false);
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    if (image->isMemObjZeroCopy() && image->mappingOnCpuAllowed()) {
        GetInfoHelper::set(imageSlicePitch, image->getImageDesc().image_slice_pitch);
        if (image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            // There are differences in qPitch programming between Gen8 vs Gen9+ devices.
            // For Gen8 qPitch is distance in rows while Gen9+ it is in pixels.
            // Minimum value of qPitch is 4 and this causes slicePitch = 4*rowPitch on Gen8.
            // To allow zero-copy we have to tell what is correct value rowPitch which should equal to slicePitch.
            GetInfoHelper::set(imageRowPitch, image->getImageDesc().image_slice_pitch);
        } else {
            GetInfoHelper::set(imageRowPitch, image->getImageDesc().image_row_pitch);
        }
    } else {
        GetInfoHelper::set(imageSlicePitch, image->getHostPtrSlicePitch());
        GetInfoHelper::set(imageRowPitch, image->getHostPtrRowPitch());
    }
    if (Image::hasSlices(image->peekClMemObjType()) == false) {
        GetInfoHelper::set(imageSlicePitch, static_cast<size_t>(0));
    }
    return enqueueMapMemObject(transferProperties, eventsRequest, errcodeRet);
}

cl_int CommandQueue::enqueueUnmapMemObject(MemObj *memObj, void *mappedPtr, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TransferProperties transferProperties(memObj, CL_COMMAND_UNMAP_MEM_OBJECT, 0, false, nullptr, nullptr, mappedPtr, false);
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    return enqueueUnmapMemObject(transferProperties, eventsRequest);
}

void CommandQueue::enqueueBlockedMapUnmapOperation(const cl_event *eventWaitList,
                                                   size_t numEventsInWaitlist,
                                                   MapOperationType opType,
                                                   MemObj *memObj,
                                                   MemObjSizeArray &copySize,
                                                   MemObjOffsetArray &copyOffset,
                                                   bool readOnly,
                                                   EventBuilder &externalEventBuilder) {
    EventBuilder internalEventBuilder;
    EventBuilder *eventBuilder;
    // check if event will be exposed externally
    if (externalEventBuilder.getEvent()) {
        externalEventBuilder.getEvent()->incRefInternal();
        eventBuilder = &externalEventBuilder;
    } else {
        // it will be an internal event
        internalEventBuilder.create<VirtualEvent>(this, context);
        eventBuilder = &internalEventBuilder;
    }

    //store task data in event
    auto cmd = std::unique_ptr<Command>(new CommandMapUnmap(opType, *memObj, copySize, copyOffset, readOnly, *this));
    eventBuilder->getEvent()->setCommand(std::move(cmd));

    //bind output event with input events
    eventBuilder->addParentEvents(ArrayRef<const cl_event>(eventWaitList, numEventsInWaitlist));
    eventBuilder->addParentEvent(this->virtualEvent);
    eventBuilder->finalize();

    if (this->virtualEvent) {
        this->virtualEvent->decRefInternal();
    }
    this->virtualEvent = eventBuilder->getEvent();
}

bool CommandQueue::setupDebugSurface(Kernel *kernel) {
    auto debugSurface = getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();

    if (!debugSurface) {
        debugSurface = getGpgpuCommandStreamReceiver().allocateDebugSurface(SipKernel::maxDbgSurfaceSize);
    }

    DEBUG_BREAK_IF(!kernel->requiresSshForBuffers());

    auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                  kernel->getKernelInfo().patchInfo.pAllocateSystemThreadSurface->Offset);
    void *addressToPatch = reinterpret_cast<void *>(debugSurface->getGpuAddress());
    size_t sizeToPatch = debugSurface->getUnderlyingBufferSize();
    Buffer::setSurfaceState(&device->getDevice(), surfaceState, sizeToPatch, addressToPatch, 0, debugSurface, 0, 0);
    return true;
}

IndirectHeap &CommandQueue::getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize) {
    return getGpgpuCommandStreamReceiver().getIndirectHeap(heapType, minRequiredSize);
}

void CommandQueue::allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap) {
    getGpgpuCommandStreamReceiver().allocateHeapMemory(heapType, minRequiredSize, indirectHeap);
}

void CommandQueue::releaseIndirectHeap(IndirectHeap::Type heapType) {
    getGpgpuCommandStreamReceiver().releaseIndirectHeap(heapType);
}

void CommandQueue::obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies) {
    auto allocator = getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();

    previousNodes.swapNodes(*timestampPacketContainer);
    previousNodes.resolveDependencies(clearAllDependencies);

    DEBUG_BREAK_IF(timestampPacketContainer->peekNodes().size() > 0);

    for (size_t i = 0; i < numberOfNodes; i++) {
        timestampPacketContainer->add(allocator->getTag());
    }
}

size_t CommandQueue::estimateTimestampPacketNodesCount(const MultiDispatchInfo &dispatchInfo) const {
    size_t nodesCount = dispatchInfo.size();
    auto mainKernel = dispatchInfo.peekMainKernel();
    if (mainKernel->requiresCacheFlushCommand(*this)) {
        nodesCount++;
    }
    return nodesCount;
}

bool CommandQueue::bufferCpuCopyAllowed(Buffer *buffer, cl_command_type commandType, cl_bool blocking, size_t size, void *ptr,
                                        cl_uint numEventsInWaitList, const cl_event *eventWaitList) {

    if (buffer->getMemoryManager() && buffer->getMemoryManager()->isCpuCopyRequired(ptr)) {
        return true;
    }

    auto debugVariableSet = false;
    // Requested by debug variable or allowed by Buffer
    if (CL_COMMAND_READ_BUFFER == commandType && DebugManager.flags.DoCpuCopyOnReadBuffer.get() != -1) {
        if (DebugManager.flags.DoCpuCopyOnReadBuffer.get() == 0) {
            return false;
        }
        debugVariableSet = true;
    }
    if (CL_COMMAND_WRITE_BUFFER == commandType && DebugManager.flags.DoCpuCopyOnWriteBuffer.get() != -1) {
        if (DebugManager.flags.DoCpuCopyOnWriteBuffer.get() == 0) {
            return false;
        }
        debugVariableSet = true;
    }

    //if we are blocked by user events, we can't service the call on CPU
    if (Event::checkUserEventDependencies(numEventsInWaitList, eventWaitList)) {
        return false;
    }

    //check if buffer is compatible
    if (!buffer->isReadWriteOnCpuAllowed()) {
        return false;
    }

    if (debugVariableSet) {
        return true;
    }

    //non blocking transfers are not expected to be serviced by CPU
    //we do not want to artifically stall the pipeline to allow CPU access
    if (blocking == CL_FALSE) {
        return false;
    }

    //check if it is beneficial to do transfer on CPU
    if (!buffer->isReadWriteOnCpuPreffered(ptr, size)) {
        return false;
    }

    //make sure that event wait list is empty
    if (numEventsInWaitList == 0) {
        return true;
    }

    return false;
}

bool CommandQueue::queueDependenciesClearRequired() const {
    return isOOQEnabled() || DebugManager.flags.OmitTimestampPacketDependencies.get();
}

bool CommandQueue::blitEnqueueAllowed(cl_command_type cmdType) const {
    bool blitAllowed = device->getHardwareInfo().capabilityTable.blitterOperationsSupported;

    if (DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.get() != -1) {
        blitAllowed &= !!DebugManager.flags.EnableBlitterOperationsForReadWriteBuffers.get();
    }

    bool commandAllowed = (CL_COMMAND_READ_BUFFER == cmdType) || (CL_COMMAND_WRITE_BUFFER == cmdType) ||
                          (CL_COMMAND_COPY_BUFFER == cmdType);

    return commandAllowed && blitAllowed;
}

bool CommandQueue::isBlockedCommandStreamRequired(uint32_t commandType, const EventsRequest &eventsRequest, bool blockedQueue) const {
    if (!blockedQueue) {
        return false;
    }

    if (isCacheFlushCommand(commandType) || !isCommandWithoutKernel(commandType)) {
        return true;
    }

    if ((CL_COMMAND_BARRIER == commandType || CL_COMMAND_MARKER == commandType) &&
        getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {

        for (size_t i = 0; i < eventsRequest.numEventsInWaitList; i++) {
            auto waitlistEvent = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
            if (waitlistEvent->getTimestampPacketNodes()) {
                return true;
            }
        }
    }

    return false;
}

void CommandQueue::aubCaptureHook(bool &blocking, bool &clearAllDependencies, const MultiDispatchInfo &multiDispatchInfo) {
    if (DebugManager.flags.AUBDumpSubCaptureMode.get()) {
        auto status = getGpgpuCommandStreamReceiver().checkAndActivateAubSubCapture(multiDispatchInfo);
        if (!status.isActive) {
            // make each enqueue blocking when subcapture is not active to split batch buffer
            blocking = true;
        } else if (!status.wasActiveInPreviousEnqueue) {
            // omit timestamp packet dependencies dependencies upon subcapture activation
            clearAllDependencies = true;
        }
    }

    if (getGpgpuCommandStreamReceiver().getType() > CommandStreamReceiverType::CSR_HW) {
        for (auto &dispatchInfo : multiDispatchInfo) {
            auto kernelName = dispatchInfo.getKernel()->getKernelInfo().name;
            getGpgpuCommandStreamReceiver().addAubComment(kernelName.c_str());
        }
    }
}
} // namespace NEO

/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"

#include "core/helpers/ptr_math.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/convert_color.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/queue_helpers.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/utilities/api_intercept.h"
#include "runtime/utilities/tag_allocator.h"

#include "CL/cl_ext.h"

#include <map>

namespace NEO {

// Global table of create functions
CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE] = {};

CommandQueue *CommandQueue::create(Context *context,
                                   Device *device,
                                   const cl_queue_properties *properties,
                                   cl_int &retVal) {
    retVal = CL_SUCCESS;

    auto funcCreate = commandQueueFactory[device->getRenderCoreFamily()];
    DEBUG_BREAK_IF(nullptr == funcCreate);

    return funcCreate(context, device, properties);
}

CommandQueue::CommandQueue() : CommandQueue(nullptr, nullptr, 0) {
}

CommandQueue::CommandQueue(Context *context, Device *deviceId, const cl_queue_properties *properties)
    : context(context), device(deviceId) {
    if (context) {
        context->incRefInternal();
    }

    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    flushStamp.reset(new FlushStampTracker(true));

    if (device) {
        engine = &device->getDefaultEngine();
        if (getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
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
        auto storageForAllocation = getCommandStreamReceiver().getInternalAllocationStorage();

        if (commandStream) {
            storageForAllocation->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream->getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }
        delete commandStream;

        if (perfConfigurationData) {
            delete perfConfigurationData;
        }
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

CommandStreamReceiver &CommandQueue::getCommandStreamReceiver() const {
    return *engine->commandStreamReceiver;
}

uint32_t CommandQueue::getHwTag() const {
    uint32_t tag = *getHwTagAddress();
    return tag;
}

volatile uint32_t *CommandQueue::getHwTagAddress() const {
    return getCommandStreamReceiver().getTagAddress();
}

bool CommandQueue::isCompleted(uint32_t taskCount) const {
    uint32_t tag = getHwTag();
    DEBUG_BREAK_IF(tag == Event::eventNotReady);
    return tag >= taskCount;
}

void CommandQueue::waitUntilComplete(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep) {
    WAIT_ENTER()

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Waiting for taskCount:", taskCountToWait);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", getHwTag());

    bool forcePowerSavingMode = this->throttle == QueueThrottle::LOW;

    getCommandStreamReceiver().waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, forcePowerSavingMode);

    DEBUG_BREAK_IF(getHwTag() < taskCountToWait);
    latestTaskCountWaited = taskCountToWait;
    WAIT_LEAVE()
}

bool CommandQueue::isQueueBlocked() {
    TakeOwnershipWrapper<CommandQueue> takeOwnershipWrapper(*this);
    //check if we have user event and if so, if it is in blocked state.
    if (this->virtualEvent) {
        if (this->virtualEvent->peekExecutionStatus() <= CL_COMPLETE) {
            UNRECOVERABLE_IF(this->virtualEvent == nullptr);

            if (this->virtualEvent->isStatusCompletedByTermination() == false) {
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
                taskLevel = getCommandStreamReceiver().peekTaskLevel();
            }

            DebugManager.log(DebugManager.flags.EventsDebugEnable.get(), "isQueueBlocked taskLevel change from", taskLevel, "to new from virtualEvent", this->virtualEvent, "new tasklevel", this->virtualEvent->taskLevel.load());

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
    getCommandStreamReceiver().ensureCommandBufferAllocation(*commandStream, minRequiredSize, additionalAllocationSize);
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
    if (completionStamp.taskCount != Event::eventNotReady) {
        taskCount = completionStamp.taskCount;
    }
    flushStamp->setStamp(completionStamp.flushStamp);
    this->taskLevel = completionStamp.taskLevel;
}

bool CommandQueue::setPerfCountersEnabled(bool perfCountersEnabled, cl_uint configuration) {
    DEBUG_BREAK_IF(device == nullptr);
    if (perfCountersEnabled == this->perfCountersEnabled) {
        return true;
    }
    auto perfCounters = device->getPerformanceCounters();
    if (perfCountersEnabled) {
        perfCounters->enable();
        if (!perfCounters->isAvailable()) {
            perfCounters->shutdown();
            return false;
        }
        perfConfigurationData = perfCounters->getPmRegsCfg(configuration);
        if (perfConfigurationData == nullptr) {
            perfCounters->shutdown();
            return false;
        }
        InstrReadRegsCfg *pUserCounters = &perfConfigurationData->ReadRegs;
        for (uint32_t i = 0; i < pUserCounters->RegsCount; ++i) {
            perfCountersUserRegistersNumber++;
            if (pUserCounters->Reg[i].BitSize > 32) {
                perfCountersUserRegistersNumber++;
            }
        }
    } else {
        if (perfCounters->isAvailable()) {
            perfCounters->shutdown();
        }
    }
    this->perfCountersConfig = configuration;
    this->perfCountersEnabled = perfCountersEnabled;

    return true;
}

PerformanceCounters *CommandQueue::getPerfCounters() {
    return device->getPerformanceCounters();
}

bool CommandQueue::sendPerfCountersConfig() {
    return getPerfCounters()->sendPmRegsCfgCommands(perfConfigurationData, &perfCountersRegsCfgHandle, &perfCountersRegsCfgPending);
}

cl_int CommandQueue::enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest) {
    cl_int retVal = CL_SUCCESS;

    MapInfo unmapInfo;
    if (!memObj->findMappedPtr(mappedPtr, unmapInfo)) {
        return CL_INVALID_VALUE;
    }

    if (!unmapInfo.readOnly) {
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
            bool mustCallFinish = true;
            if (!(image->getFlags() & CL_MEM_USE_HOST_PTR)) {
                mustCallFinish = true;
            } else {
                mustCallFinish = (CommandQueue::getTaskLevelFromWaitList(this->taskLevel, eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList) != Event::eventNotReady);
            }
            if (mustCallFinish) {
                finish(true);
            }
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
    void *basePtr = transferProperties.memObj->getBasePtrForMap();
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
    auto cmd = std::unique_ptr<Command>(new CommandMapUnmap(opType, *memObj, copySize, copyOffset, readOnly, getCommandStreamReceiver(), *this));
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
    auto debugSurface = getCommandStreamReceiver().getDebugSurfaceAllocation();

    if (!debugSurface) {
        debugSurface = getCommandStreamReceiver().allocateDebugSurface(SipKernel::maxDbgSurfaceSize);
    }

    DEBUG_BREAK_IF(!kernel->requiresSshForBuffers());

    auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                  kernel->getKernelInfo().patchInfo.pAllocateSystemThreadSurface->Offset);
    void *addressToPatch = reinterpret_cast<void *>(debugSurface->getGpuAddress());
    size_t sizeToPatch = debugSurface->getUnderlyingBufferSize();
    Buffer::setSurfaceState(device, surfaceState, sizeToPatch, addressToPatch, debugSurface);
    return true;
}

IndirectHeap &CommandQueue::getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize) {
    return getCommandStreamReceiver().getIndirectHeap(heapType, minRequiredSize);
}

void CommandQueue::allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap) {
    getCommandStreamReceiver().allocateHeapMemory(heapType, minRequiredSize, indirectHeap);
}

void CommandQueue::releaseIndirectHeap(IndirectHeap::Type heapType) {
    getCommandStreamReceiver().releaseIndirectHeap(heapType);
}

void CommandQueue::dispatchAuxTranslation(MultiDispatchInfo &multiDispatchInfo, MemObjsForAuxTranslation &memObjsForAuxTranslation,
                                          AuxTranslationDirection auxTranslationDirection) {
    if (!multiDispatchInfo.empty()) {
        multiDispatchInfo.rbegin()->setPipeControlRequired(true);
    }
    auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, getContext(), getDevice());
    BuiltinDispatchInfoBuilder::BuiltinOpParams dispatchParams;

    dispatchParams.memObjsForAuxTranslation = &memObjsForAuxTranslation;
    dispatchParams.auxTranslationDirection = auxTranslationDirection;

    builder.buildDispatchInfos(multiDispatchInfo, dispatchParams);

    multiDispatchInfo.rbegin()->setPipeControlRequired(true);
}

void CommandQueue::obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies) {
    auto allocator = getCommandStreamReceiver().getTimestampPacketAllocator();

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
    // Requested by debug variable or allowed by Buffer
    bool debugVariableSet = (CL_COMMAND_READ_BUFFER == commandType && DebugManager.flags.DoCpuCopyOnReadBuffer.get()) ||
                            (CL_COMMAND_WRITE_BUFFER == commandType && DebugManager.flags.DoCpuCopyOnWriteBuffer.get());

    return (debugVariableSet && !Event::checkUserEventDependencies(numEventsInWaitList, eventWaitList) &&
            buffer->getGraphicsAllocation()->getAllocationType() != GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) ||
           buffer->isReadWriteOnCpuAllowed(blocking, numEventsInWaitList, ptr, size);
}

cl_int CommandQueue::enqueueReadWriteBufferWithBlitTransfer(cl_command_type commandType, Buffer *buffer,
                                                            size_t offset, size_t size, void *ptr, cl_uint numEventsInWaitList,
                                                            const cl_event *eventWaitList, cl_event *event) {
    auto blitCommandStreamReceiver = context->getCommandStreamReceiverForBlitOperation(*buffer);
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    TimestampPacketContainer previousTimestampPacketNodes;
    CsrDependencies csrDependencies;

    csrDependencies.fillFromEventsRequestAndMakeResident(eventsRequest, *blitCommandStreamReceiver,
                                                         CsrDependencies::DependenciesType::All);

    obtainNewTimestampPacketNodes(1, previousTimestampPacketNodes, queueDependenciesClearRequired());
    csrDependencies.push_back(&previousTimestampPacketNodes);

    auto copyDirection = (CL_COMMAND_WRITE_BUFFER == commandType) ? BlitterConstants::BlitWithHostPtrDirection::FromHostPtr
                                                                  : BlitterConstants::BlitWithHostPtrDirection::ToHostPtr;
    blitCommandStreamReceiver->blitWithHostPtr(*buffer, ptr, true, offset, size, copyDirection, csrDependencies, *timestampPacketContainer);
    return CL_SUCCESS;
}

bool CommandQueue::queueDependenciesClearRequired() const {
    return isOOQEnabled() || DebugManager.flags.OmitTimestampPacketDependencies.get();
}
} // namespace NEO

/*
 * Copyright (C) 2018-2022 Intel Corporation
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
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/program/printf_handler.h"

#include "CL/cl_ext.h"

#include <limits>
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

CommandQueue::CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties, bool internalUsage)
    : context(context), device(device) {
    if (context) {
        context->incRefInternal();
    }

    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    flushStamp.reset(new FlushStampTracker(true));

    if (device) {
        auto &hwInfo = device->getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

        bcsAllowed = hwInfoConfig->isBlitterFullySupported(hwInfo) &&
                     hwHelper.isSubDeviceEngineSupported(hwInfo, device->getDeviceBitfield(), aub_stream::EngineType::ENGINE_BCS);

        if (bcsAllowed || device->getDefaultEngine().commandStreamReceiver->peekTimestampPacketWriteEnabled()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
            deferredTimestampPackets = std::make_unique<TimestampPacketContainer>();
        }

        auto deferCmdQBcsInitialization = hwInfo.featureTable.ftrBcsInfo.count() > 1u;

        if (DebugManager.flags.DeferCmdQBcsInitialization.get() != -1) {
            deferCmdQBcsInitialization = DebugManager.flags.DeferCmdQBcsInitialization.get();
        }

        if (!deferCmdQBcsInitialization) {
            this->constructBcsEngine(internalUsage);
        }
    }

    storeProperties(properties);
    processProperties(properties);
}

CommandQueue::~CommandQueue() {
    if (virtualEvent) {
        UNRECOVERABLE_IF(this->virtualEvent->getCommandQueue() != this && this->virtualEvent->getCommandQueue() != nullptr);
        virtualEvent->decRefInternal();
    }

    if (device) {
        if (commandStream) {
            auto storageForAllocation = gpgpuEngine->commandStreamReceiver->getInternalAllocationStorage();
            storageForAllocation->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream->getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }
        delete commandStream;

        if (this->perfCountersEnabled) {
            device->getPerformanceCounters()->shutdown();
        }

        if (auto mainBcs = bcsEngines[0]; mainBcs != nullptr) {
            auto &selectorCopyEngine = device->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
            EngineHelpers::releaseBcsEngineType(mainBcs->getEngineType(), selectorCopyEngine);
        }
    }

    timestampPacketContainer.reset();
    // for normal queue, decrement ref count on context
    // special queue is owned by context so ref count doesn't have to be decremented
    if (context && !isSpecialCommandQueue) {
        context->decRefInternal();
    }
    gtpinRemoveCommandQueue(this);
}

void CommandQueue::initializeGpgpu() const {
    if (gpgpuEngine == nullptr) {
        auto &hwInfo = device->getDevice().getHardwareInfo();
        auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);

        auto engineRoundRobinAvailable = hwHelper.isAssignEngineRoundRobinSupported(hwInfo) &&
                                         this->isAssignEngineRoundRobinEnabled();

        if (DebugManager.flags.EnableCmdQRoundRobindEngineAssign.get() != -1) {
            engineRoundRobinAvailable = DebugManager.flags.EnableCmdQRoundRobindEngineAssign.get();
        }

        auto assignEngineRoundRobin =
            !this->isSpecialCommandQueue &&
            !this->queueFamilySelected &&
            !(getCmdQueueProperties<cl_queue_priority_khr>(propertiesVector.data(), CL_QUEUE_PRIORITY_KHR) & static_cast<cl_queue_priority_khr>(CL_QUEUE_PRIORITY_LOW_KHR)) &&
            engineRoundRobinAvailable;

        if (assignEngineRoundRobin) {
            this->gpgpuEngine = &device->getDevice().getNextEngineForCommandQueue();
        } else {
            this->gpgpuEngine = &device->getDefaultEngine();
        }

        this->initializeGpgpuInternals();
    }
}

void CommandQueue::initializeGpgpuInternals() const {
    auto &hwInfo = device->getDevice().getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    if (device->getDevice().getDebugger() && !this->gpgpuEngine->commandStreamReceiver->getDebugSurfaceAllocation()) {
        auto maxDbgSurfaceSize = hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
        auto debugSurface = this->gpgpuEngine->commandStreamReceiver->allocateDebugSurface(maxDbgSurfaceSize);
        memset(debugSurface->getUnderlyingBuffer(), 0, debugSurface->getUnderlyingBufferSize());

        auto &stateSaveAreaHeader = SipKernel::getSipKernel(device->getDevice()).getStateSaveAreaHeader();
        if (stateSaveAreaHeader.size() > 0) {
            NEO::MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *debugSurface),
                                                                  device->getDevice(), debugSurface, 0, stateSaveAreaHeader.data(),
                                                                  stateSaveAreaHeader.size());
        }
    }

    gpgpuEngine->osContext->ensureContextInitialized();
    gpgpuEngine->commandStreamReceiver->initDirectSubmission();

    if (getCmdQueueProperties<cl_queue_properties>(propertiesVector.data(), CL_QUEUE_PROPERTIES) & static_cast<cl_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) && !this->gpgpuEngine->commandStreamReceiver->isUpdateTagFromWaitEnabled()) {
        this->gpgpuEngine->commandStreamReceiver->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
        if (DebugManager.flags.CsrDispatchMode.get() != 0) {
            this->gpgpuEngine->commandStreamReceiver->overrideDispatchPolicy(static_cast<DispatchMode>(DebugManager.flags.CsrDispatchMode.get()));
        }
        this->gpgpuEngine->commandStreamReceiver->enableNTo1SubmissionModel();
    }
}

CommandStreamReceiver &CommandQueue::getGpgpuCommandStreamReceiver() const {
    this->initializeGpgpu();
    return *gpgpuEngine->commandStreamReceiver;
}

CommandStreamReceiver *CommandQueue::getBcsCommandStreamReceiver(aub_stream::EngineType bcsEngineType) {
    initializeBcsEngine(isSpecial());
    const EngineControl *engine = this->bcsEngines[EngineHelpers::getBcsIndex(bcsEngineType)];
    if (engine == nullptr) {
        return nullptr;
    } else {
        return engine->commandStreamReceiver;
    }
}

CommandStreamReceiver *CommandQueue::getBcsForAuxTranslation() {
    initializeBcsEngine(isSpecial());
    for (const EngineControl *engine : this->bcsEngines) {
        if (engine != nullptr) {
            return engine->commandStreamReceiver;
        }
    }
    return nullptr;
}

CommandStreamReceiver &CommandQueue::selectCsrForBuiltinOperation(const CsrSelectionArgs &args) {
    initializeBcsEngine(isSpecial());
    if (isCopyOnly) {
        return *getBcsCommandStreamReceiver(bcsEngineTypes[0]);
    }

    if (!blitEnqueueAllowed(args)) {
        return getGpgpuCommandStreamReceiver();
    }

    bool preferBcs = true;
    aub_stream::EngineType preferredBcsEngineType = aub_stream::EngineType::NUM_ENGINES;
    switch (args.direction) {
    case TransferDirection::LocalToLocal: {
        const auto &clHwHelper = ClHwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
        preferBcs = clHwHelper.preferBlitterForLocalToLocalTransfers();
        if (auto flag = DebugManager.flags.PreferCopyEngineForCopyBufferToBuffer.get(); flag != -1) {
            preferBcs = static_cast<bool>(flag);
        }
        if (preferBcs) {
            preferredBcsEngineType = aub_stream::EngineType::ENGINE_BCS;
        }
        break;
    }
    case TransferDirection::HostToHost:
    case TransferDirection::HostToLocal:
    case TransferDirection::LocalToHost: {
        preferBcs = true;

        auto preferredBCSType = true;

        if (DebugManager.flags.AssignBCSAtEnqueue.get() != -1) {
            preferredBCSType = DebugManager.flags.AssignBCSAtEnqueue.get();
        }

        if (preferredBCSType) {
            preferredBcsEngineType = EngineHelpers::getBcsEngineType(device->getHardwareInfo(), device->getDeviceBitfield(),
                                                                     device->getSelectorCopyEngine(), false);
        }
        break;
    }
    default:
        UNRECOVERABLE_IF(true);
    }

    CommandStreamReceiver *selectedCsr = nullptr;
    if (preferBcs) {
        auto assignBCS = true;

        if (DebugManager.flags.AssignBCSAtEnqueue.get() != -1) {
            assignBCS = DebugManager.flags.AssignBCSAtEnqueue.get();
        }

        if (assignBCS) {
            selectedCsr = getBcsCommandStreamReceiver(preferredBcsEngineType);
        }

        if (selectedCsr == nullptr && !bcsEngineTypes.empty()) {
            selectedCsr = getBcsCommandStreamReceiver(bcsEngineTypes[0]);
        }
    }
    if (selectedCsr == nullptr) {
        selectedCsr = &getGpgpuCommandStreamReceiver();
    }

    UNRECOVERABLE_IF(selectedCsr == nullptr);
    return *selectedCsr;
}

void CommandQueue::constructBcsEngine(bool internalUsage) {
    if (bcsAllowed && !bcsInitialized) {
        auto &hwInfo = device->getHardwareInfo();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto &neoDevice = device->getNearestGenericSubDevice(0)->getDevice();
        auto &selectorCopyEngine = neoDevice.getSelectorCopyEngine();
        auto bcsEngineType = EngineHelpers::getBcsEngineType(hwInfo, device->getDeviceBitfield(), selectorCopyEngine, internalUsage);
        auto bcsIndex = EngineHelpers::getBcsIndex(bcsEngineType);
        auto engineUsage = (internalUsage && hwHelper.preferInternalBcsEngine()) ? EngineUsage::Internal : EngineUsage::Regular;
        bcsEngines[bcsIndex] = neoDevice.tryGetEngine(bcsEngineType, engineUsage);
        bcsEngineTypes.push_back(bcsEngineType);
        bcsInitialized = true;
        if (bcsEngines[bcsIndex]) {
            bcsEngines[bcsIndex]->osContext->ensureContextInitialized();
            bcsEngines[bcsIndex]->commandStreamReceiver->initDirectSubmission();
        }
    }
}

void CommandQueue::initializeBcsEngine(bool internalUsage) {
    constructBcsEngine(internalUsage);
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

bool CommandQueue::isCompleted(uint32_t gpgpuTaskCount, CopyEngineState bcsState) {
    DEBUG_BREAK_IF(getHwTag() == CompletionStamp::notReady);

    if (getGpgpuCommandStreamReceiver().testTaskCountReady(getHwTagAddress(), gpgpuTaskCount)) {
        if (bcsState.isValid()) {
            return *getBcsCommandStreamReceiver(bcsState.engineType)->getTagAddress() >= peekBcsTaskCount(bcsState.engineType);
        }

        return true;
    }

    return false;
}

WaitStatus CommandQueue::waitUntilComplete(uint32_t gpgpuTaskCountToWait, Range<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) {
    WAIT_ENTER()

    WaitStatus waitStatus{WaitStatus::Ready};

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Waiting for taskCount:", gpgpuTaskCountToWait);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", getHwTag());

    if (!skipWait) {
        waitStatus = getGpgpuCommandStreamReceiver().waitForTaskCountWithKmdNotifyFallback(gpgpuTaskCountToWait,
                                                                                           flushStampToWait,
                                                                                           useQuickKmdSleep,
                                                                                           this->getThrottle());
        if (waitStatus == WaitStatus::GpuHang) {
            return WaitStatus::GpuHang;
        }

        DEBUG_BREAK_IF(getHwTag() < gpgpuTaskCountToWait);

        if (gtpinIsGTPinInitialized()) {
            gtpinNotifyTaskCompletion(gpgpuTaskCountToWait);
        }

        for (const CopyEngineState &copyEngine : copyEnginesToWait) {
            auto bcsCsr = getBcsCommandStreamReceiver(copyEngine.engineType);

            waitStatus = bcsCsr->waitForTaskCountWithKmdNotifyFallback(copyEngine.taskCount, 0, false, this->getThrottle());
            if (waitStatus == WaitStatus::GpuHang) {
                return WaitStatus::GpuHang;
            }
        }
    } else if (gtpinIsGTPinInitialized()) {
        gtpinNotifyTaskCompletion(gpgpuTaskCountToWait);
    }

    for (const CopyEngineState &copyEngine : copyEnginesToWait) {
        auto bcsCsr = getBcsCommandStreamReceiver(copyEngine.engineType);

        waitStatus = bcsCsr->waitForTaskCountAndCleanTemporaryAllocationList(copyEngine.taskCount);
        if (waitStatus == WaitStatus::GpuHang) {
            return WaitStatus::GpuHang;
        }
    }

    waitStatus = cleanTemporaryAllocationList
                     ? getGpgpuCommandStreamReceiver().waitForTaskCountAndCleanTemporaryAllocationList(gpgpuTaskCountToWait)
                     : getGpgpuCommandStreamReceiver().waitForTaskCount(gpgpuTaskCountToWait);

    WAIT_LEAVE()

    return waitStatus;
}

bool CommandQueue::isQueueBlocked() {
    TakeOwnershipWrapper<CommandQueue> takeOwnershipWrapper(*this);
    // check if we have user event and if so, if it is in blocked state.
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
                // at this point we may reset queue TaskCount, since all command previous to this were aborted
                taskCount = 0;
                flushStamp->setStamp(0);
                taskLevel = getGpgpuCommandStreamReceiver().peekTaskLevel();
            }

            fileLoggerInstance().log(DebugManager.flags.EventsDebugEnable.get(), "isQueueBlocked taskLevel change from", taskLevel, "to new from virtualEvent", this->virtualEvent, "new tasklevel", this->virtualEvent->taskLevel.load());

            // close the access to virtual event, driver added only 1 ref count.
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
    return getQueueInfo(this, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

uint32_t CommandQueue::getTaskLevelFromWaitList(uint32_t taskLevel,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList) {
    for (auto iEvent = 0u; iEvent < numEventsInWaitList; ++iEvent) {
        auto pEvent = (Event *)(eventWaitList[iEvent]);
        uint32_t eventTaskLevel = pEvent->peekTaskLevel();
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

        int result = memObject->peekSharingHandler()->acquire(memObject, getDevice().getRootDeviceIndex());
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

        memObject->peekSharingHandler()->release(memObject, getDevice().getRootDeviceIndex());
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

void CommandQueue::updateFromCompletionStamp(const CompletionStamp &completionStamp, Event *outEvent) {
    DEBUG_BREAK_IF(this->taskLevel > completionStamp.taskLevel);
    DEBUG_BREAK_IF(this->taskCount > completionStamp.taskCount);
    if (completionStamp.taskCount != CompletionStamp::notReady) {
        taskCount = completionStamp.taskCount;
    }
    flushStamp->setStamp(completionStamp.flushStamp);
    this->taskLevel = completionStamp.taskLevel;

    if (outEvent) {
        outEvent->updateCompletionStamp(completionStamp.taskCount, outEvent->peekBcsTaskCountFromCommandQueue(), completionStamp.taskLevel, completionStamp.flushStamp);
        fileLoggerInstance().log(DebugManager.flags.EventsDebugEnable.get(), "updateCompletionStamp Event", outEvent, "taskLevel", outEvent->taskLevel.load());
    }
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
        memObj->getMapAllocation(getDevice().getRootDeviceIndex())->setAubWritable(true, GraphicsAllocation::defaultBank);
        memObj->getMapAllocation(getDevice().getRootDeviceIndex())->setTbxWritable(true, GraphicsAllocation::defaultBank);

        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            auto buffer = castToObject<Buffer>(memObj);

            retVal = enqueueWriteBuffer(buffer, CL_FALSE, unmapInfo.offset[0], unmapInfo.size[0], mappedPtr, memObj->getMapAllocation(getDevice().getRootDeviceIndex()),
                                        eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
        } else {
            auto image = castToObjectOrAbort<Image>(memObj);
            size_t writeOrigin[4] = {unmapInfo.offset[0], unmapInfo.offset[1], unmapInfo.offset[2], 0};
            auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
            UNRECOVERABLE_IF(mipIdx >= 4);
            writeOrigin[mipIdx] = unmapInfo.mipLevel;
            retVal = enqueueWriteImage(image, CL_FALSE, writeOrigin, &unmapInfo.size[0],
                                       image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(), mappedPtr, memObj->getMapAllocation(getDevice().getRootDeviceIndex()),
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
                                                 transferProperties.mapFlags, transferProperties.size, transferProperties.offset, transferProperties.mipLevel,
                                                 transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()))) {
        errcodeRet = CL_INVALID_OPERATION;
        return nullptr;
    }

    if (transferProperties.mapFlags == CL_MAP_WRITE_INVALIDATE_REGION) {
        errcodeRet = enqueueMarkerWithWaitList(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
    } else {
        if (transferProperties.memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            auto buffer = castToObject<Buffer>(transferProperties.memObj);
            errcodeRet = enqueueReadBuffer(buffer, transferProperties.blocking, transferProperties.offset[0], transferProperties.size[0],
                                           returnPtr, transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()), eventsRequest.numEventsInWaitList,
                                           eventsRequest.eventWaitList, eventsRequest.outEvent);
        } else {
            auto image = castToObjectOrAbort<Image>(transferProperties.memObj);
            size_t readOrigin[4] = {transferProperties.offset[0], transferProperties.offset[1], transferProperties.offset[2], 0};
            auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
            UNRECOVERABLE_IF(mipIdx >= 4);
            readOrigin[mipIdx] = transferProperties.mipLevel;
            errcodeRet = enqueueReadImage(image, transferProperties.blocking, readOrigin, &transferProperties.size[0],
                                          image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(),
                                          returnPtr, transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()), eventsRequest.numEventsInWaitList,
                                          eventsRequest.eventWaitList, eventsRequest.outEvent);
        }
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
    TransferProperties transferProperties(buffer, CL_COMMAND_MAP_BUFFER, mapFlags, blockingMap != CL_FALSE, &offset, &size, nullptr, false, getDevice().getRootDeviceIndex());
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
                                          const_cast<size_t *>(origin), const_cast<size_t *>(region), nullptr, false, getDevice().getRootDeviceIndex());
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
    TransferProperties transferProperties(memObj, CL_COMMAND_UNMAP_MEM_OBJECT, 0, false, nullptr, nullptr, mappedPtr, false, getDevice().getRootDeviceIndex());
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

    // store task data in event
    auto cmd = std::unique_ptr<Command>(new CommandMapUnmap(opType, *memObj, copySize, copyOffset, readOnly, *this));
    eventBuilder->getEvent()->setCommand(std::move(cmd));

    // bind output event with input events
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
    auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                  kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful);
    void *addressToPatch = reinterpret_cast<void *>(debugSurface->getGpuAddress());
    size_t sizeToPatch = debugSurface->getUnderlyingBufferSize();
    Buffer::setSurfaceState(&device->getDevice(), surfaceState, false, false, sizeToPatch,
                            addressToPatch, 0, debugSurface, 0, 0,
                            kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics,
                            kernel->areMultipleSubDevicesInContext());
    return true;
}

bool CommandQueue::validateCapability(cl_command_queue_capabilities_intel capability) const {
    return this->queueCapabilities == CL_QUEUE_DEFAULT_CAPABILITIES_INTEL || isValueSet(this->queueCapabilities, capability);
}

bool CommandQueue::validateCapabilitiesForEventWaitList(cl_uint numEventsInWaitList, const cl_event *waitList) const {
    for (cl_uint eventIndex = 0u; eventIndex < numEventsInWaitList; eventIndex++) {
        const Event *event = castToObject<Event>(waitList[eventIndex]);
        if (event->isUserEvent()) {
            continue;
        }

        const CommandQueue *eventCommandQueue = event->getCommandQueue();
        const bool crossQueue = this != eventCommandQueue;
        const cl_command_queue_capabilities_intel createCap = crossQueue ? CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL
                                                                         : CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL;
        const cl_command_queue_capabilities_intel waitCap = crossQueue ? CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL
                                                                       : CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL;
        if (!validateCapability(waitCap) || !eventCommandQueue->validateCapability(createCap)) {
            return false;
        }
    }

    return true;
}

bool CommandQueue::validateCapabilityForOperation(cl_command_queue_capabilities_intel capability,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *waitList,
                                                  const cl_event *outEvent) const {
    const bool operationValid = validateCapability(capability);
    const bool waitListValid = validateCapabilitiesForEventWaitList(numEventsInWaitList, waitList);
    const bool outEventValid = outEvent == nullptr ||
                               validateCapability(CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL) ||
                               validateCapability(CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL);
    return operationValid && waitListValid && outEventValid;
}

cl_uint CommandQueue::getQueueFamilyIndex() const {
    if (isQueueFamilySelected()) {
        return queueFamilyIndex;
    } else {
        const auto &hwInfo = device->getHardwareInfo();
        const auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        const auto engineGroupType = hwHelper.getEngineGroupType(getGpgpuEngine().getEngineType(), getGpgpuEngine().getEngineUsage(), hwInfo);
        const auto familyIndex = device->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
        return static_cast<cl_uint>(familyIndex);
    }
}

void CommandQueue::updateBcsTaskCount(aub_stream::EngineType bcsEngineType, uint32_t newBcsTaskCount) {
    CopyEngineState &state = bcsStates[EngineHelpers::getBcsIndex(bcsEngineType)];
    state.engineType = bcsEngineType;
    state.taskCount = newBcsTaskCount;
}

uint32_t CommandQueue::peekBcsTaskCount(aub_stream::EngineType bcsEngineType) const {
    const CopyEngineState &state = bcsStates[EngineHelpers::getBcsIndex(bcsEngineType)];
    return state.taskCount;
}

bool CommandQueue::isTextureCacheFlushNeeded(uint32_t commandType) const {
    return (commandType == CL_COMMAND_COPY_IMAGE || commandType == CL_COMMAND_WRITE_IMAGE) && getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled();
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

void CommandQueue::obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies, CommandStreamReceiver &csr) {
    TagAllocatorBase *allocator = csr.getTimestampPacketAllocator();

    previousNodes.swapNodes(*timestampPacketContainer);

    if (clearAllDependencies) {
        previousNodes.moveNodesToNewContainer(*deferredTimestampPackets);
    }

    DEBUG_BREAK_IF(timestampPacketContainer->peekNodes().size() > 0);

    for (size_t i = 0; i < numberOfNodes; i++) {
        timestampPacketContainer->add(allocator->getTag());
    }
}

size_t CommandQueue::estimateTimestampPacketNodesCount(const MultiDispatchInfo &dispatchInfo) const {
    size_t nodesCount = dispatchInfo.size();
    auto mainKernel = dispatchInfo.peekMainKernel();
    if (obtainTimestampPacketForCacheFlush(mainKernel->requiresCacheFlushCommand(*this))) {
        nodesCount++;
    }
    return nodesCount;
}

bool CommandQueue::bufferCpuCopyAllowed(Buffer *buffer, cl_command_type commandType, cl_bool blocking, size_t size, void *ptr,
                                        cl_uint numEventsInWaitList, const cl_event *eventWaitList) {

    const auto &hwInfo = device->getHardwareInfo();
    const auto &hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (CL_COMMAND_READ_BUFFER == commandType && hwInfoConfig->isCpuCopyNecessary(ptr, buffer->getMemoryManager())) {
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

    // if we are blocked by user events, we can't service the call on CPU
    if (Event::checkUserEventDependencies(numEventsInWaitList, eventWaitList)) {
        return false;
    }

    // check if buffer is compatible
    if (!buffer->isReadWriteOnCpuAllowed(device->getDevice())) {
        return false;
    }

    if (buffer->getMemoryManager() && buffer->getMemoryManager()->isCpuCopyRequired(ptr)) {
        return true;
    }

    if (debugVariableSet) {
        return true;
    }

    // non blocking transfers are not expected to be serviced by CPU
    // we do not want to artifically stall the pipeline to allow CPU access
    if (blocking == CL_FALSE) {
        return false;
    }

    // check if it is beneficial to do transfer on CPU
    if (!buffer->isReadWriteOnCpuPreferred(ptr, size, getDevice())) {
        return false;
    }

    // make sure that event wait list is empty
    if (numEventsInWaitList == 0) {
        return true;
    }

    return false;
}

bool CommandQueue::queueDependenciesClearRequired() const {
    return isOOQEnabled() || DebugManager.flags.OmitTimestampPacketDependencies.get();
}

bool CommandQueue::blitEnqueueAllowed(const CsrSelectionArgs &args) const {
    bool blitEnqueueAllowed = getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() || this->isCopyOnly;
    if (DebugManager.flags.EnableBlitterForEnqueueOperations.get() != -1) {
        blitEnqueueAllowed = DebugManager.flags.EnableBlitterForEnqueueOperations.get();
    }
    if (!blitEnqueueAllowed) {
        return false;
    }

    switch (args.cmdType) {
    case CL_COMMAND_READ_BUFFER:
    case CL_COMMAND_WRITE_BUFFER:
    case CL_COMMAND_COPY_BUFFER:
    case CL_COMMAND_READ_BUFFER_RECT:
    case CL_COMMAND_WRITE_BUFFER_RECT:
    case CL_COMMAND_COPY_BUFFER_RECT:
    case CL_COMMAND_SVM_MEMCPY:
    case CL_COMMAND_SVM_MAP:
    case CL_COMMAND_SVM_UNMAP:
        return true;
    case CL_COMMAND_READ_IMAGE:
        return blitEnqueueImageAllowed(args.srcResource.imageOrigin, args.size, *args.srcResource.image);
    case CL_COMMAND_WRITE_IMAGE:
        return blitEnqueueImageAllowed(args.dstResource.imageOrigin, args.size, *args.dstResource.image);

    case CL_COMMAND_COPY_IMAGE:
        return blitEnqueueImageAllowed(args.srcResource.imageOrigin, args.size, *args.srcResource.image) &&
               blitEnqueueImageAllowed(args.dstResource.imageOrigin, args.size, *args.dstResource.image);

    default:
        return false;
    }
}

bool CommandQueue::blitEnqueueImageAllowed(const size_t *origin, const size_t *region, const Image &image) const {
    const auto &hwInfo = device->getHardwareInfo();
    const auto &hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto blitEnqueueImageAllowed = hwInfoConfig->isBlitterForImagesSupported();

    if (DebugManager.flags.EnableBlitterForEnqueueImageOperations.get() != -1) {
        blitEnqueueImageAllowed = DebugManager.flags.EnableBlitterForEnqueueImageOperations.get();
    }

    blitEnqueueImageAllowed &= !isMipMapped(image.getImageDesc());

    const auto &defaultGmm = image.getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm();
    if (defaultGmm != nullptr) {
        auto isTile64 = defaultGmm->gmmResourceInfo->getResourceFlags()->Info.Tile64;
        auto imageType = image.getImageDesc().image_type;
        if (isTile64 && (imageType == CL_MEM_OBJECT_IMAGE3D)) {
            blitEnqueueImageAllowed &= hwInfoConfig->isTile64With3DSurfaceOnBCSSupported(hwInfo);
        }
    }

    return blitEnqueueImageAllowed;
}

bool CommandQueue::isBlockedCommandStreamRequired(uint32_t commandType, const EventsRequest &eventsRequest, bool blockedQueue, bool isMarkerWithProfiling) const {
    if (!blockedQueue) {
        return false;
    }

    if (isCacheFlushCommand(commandType) || !isCommandWithoutKernel(commandType) || isMarkerWithProfiling) {
        return true;
    }

    if (CL_COMMAND_BARRIER == commandType || CL_COMMAND_MARKER == commandType) {
        auto timestampPacketWriteEnabled = getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled();
        if (timestampPacketWriteEnabled || context->getRootDeviceIndices().size() > 1) {

            for (size_t i = 0; i < eventsRequest.numEventsInWaitList; i++) {
                auto waitlistEvent = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
                if (timestampPacketWriteEnabled && waitlistEvent->getTimestampPacketNodes()) {
                    return true;
                }
                if (waitlistEvent->getCommandQueue() && waitlistEvent->getCommandQueue()->getDevice().getRootDeviceIndex() != this->getDevice().getRootDeviceIndex()) {
                    return true;
                }
            }
        }
    }

    return false;
}

void CommandQueue::storeProperties(const cl_queue_properties *properties) {
    if (properties) {
        for (size_t i = 0; properties[i] != 0; i += 2) {
            propertiesVector.push_back(properties[i]);
            propertiesVector.push_back(properties[i + 1]);
        }
        propertiesVector.push_back(0);
    }
}

void CommandQueue::processProperties(const cl_queue_properties *properties) {
    if (properties != nullptr) {
        bool specificEngineSelected = false;
        cl_uint selectedQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
        cl_uint selectedQueueIndex = std::numeric_limits<uint32_t>::max();

        for (auto currentProperties = properties; *currentProperties != 0; currentProperties += 2) {
            switch (*currentProperties) {
            case CL_QUEUE_FAMILY_INTEL:
                selectedQueueFamilyIndex = static_cast<cl_uint>(*(currentProperties + 1));
                specificEngineSelected = true;
                break;
            case CL_QUEUE_INDEX_INTEL:
                selectedQueueIndex = static_cast<cl_uint>(*(currentProperties + 1));
                auto nodeOrdinal = DebugManager.flags.NodeOrdinal.get();
                if (nodeOrdinal != -1) {
                    int currentEngineIndex = 0;
                    const HardwareInfo &hwInfo = getDevice().getHardwareInfo();
                    const HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

                    auto engineGroupType = hwHelper.getEngineGroupType(static_cast<aub_stream::EngineType>(nodeOrdinal), EngineUsage::Regular, hwInfo);
                    selectedQueueFamilyIndex = static_cast<cl_uint>(getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType));
                    const auto &engines = getDevice().getRegularEngineGroups()[selectedQueueFamilyIndex].engines;
                    for (const auto &engine : engines) {
                        if (engine.getEngineType() == static_cast<aub_stream::EngineType>(nodeOrdinal)) {
                            selectedQueueIndex = currentEngineIndex;
                            break;
                        }
                        currentEngineIndex++;
                    }
                }
                specificEngineSelected = true;
                break;
            }
        }

        if (specificEngineSelected) {
            this->queueFamilySelected = true;
            if (!getDevice().hasRootCsr()) {
                const auto &engine = getDevice().getRegularEngineGroups()[selectedQueueFamilyIndex].engines[selectedQueueIndex];
                auto engineType = engine.getEngineType();
                auto engineUsage = engine.getEngineUsage();
                if ((DebugManager.flags.EngineUsageHint.get() != -1) &&
                    (getDevice().tryGetEngine(engineType, static_cast<EngineUsage>(DebugManager.flags.EngineUsageHint.get())) != nullptr)) {
                    engineUsage = static_cast<EngineUsage>(DebugManager.flags.EngineUsageHint.get());
                }
                this->overrideEngine(engineType, engineUsage);
                this->queueCapabilities = getClDevice().getDeviceInfo().queueFamilyProperties[selectedQueueFamilyIndex].capabilities;
                this->queueFamilyIndex = selectedQueueFamilyIndex;
                this->queueIndexWithinFamily = selectedQueueIndex;
            }
        }
    }
    requiresCacheFlushAfterWalker = device && (device->getDeviceInfo().parentDevice != nullptr);
}

void CommandQueue::overrideEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    const HardwareInfo &hwInfo = getDevice().getHardwareInfo();
    const HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    const EngineGroupType engineGroupType = hwHelper.getEngineGroupType(engineType, engineUsage, hwInfo);
    const bool isEngineCopyOnly = EngineHelper::isCopyOnlyEngineType(engineGroupType);

    if (isEngineCopyOnly) {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
        bcsEngines[EngineHelpers::getBcsIndex(engineType)] = &device->getEngine(engineType, EngineUsage::Regular);
        bcsEngineTypes = {engineType};
        timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
        deferredTimestampPackets = std::make_unique<TimestampPacketContainer>();
        isCopyOnly = true;
        bcsInitialized = true;
    } else {
        gpgpuEngine = &device->getEngine(engineType, engineUsage);
    }
}

void CommandQueue::aubCaptureHook(bool &blocking, bool &clearAllDependencies, const MultiDispatchInfo &multiDispatchInfo) {
    if (DebugManager.flags.AUBDumpSubCaptureMode.get()) {
        auto status = getGpgpuCommandStreamReceiver().checkAndActivateAubSubCapture(multiDispatchInfo.empty() ? "" : multiDispatchInfo.peekMainKernel()->getDescriptor().kernelMetadata.kernelName);
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
            auto &kernelName = dispatchInfo.getKernel()->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName;
            getGpgpuCommandStreamReceiver().addAubComment(kernelName.c_str());
        }
    }
}

void CommandQueue::assignDataToOverwrittenBcsNode(TagNodeBase *node) {
    std::array<uint32_t, 8u> timestampData;
    timestampData.fill(std::numeric_limits<uint32_t>::max());
    if (node->refCountFetchSub(0) <= 2) { // One ref from deferred container and one from bcs barrier container it is going to be released from
        for (uint32_t i = 0; i < node->getPacketsUsed(); i++) {
            node->assignDataToAllTimestamps(i, timestampData.data());
        }
    }
}

bool CommandQueue::isWaitForTimestampsEnabled() const {
    const auto &hwHelper = HwHelper::get(getDevice().getHardwareInfo().platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(getDevice().getHardwareInfo().platform.eProductFamily);
    auto enabled = CommandQueue::isTimestampWaitEnabled();
    enabled &= hwHelper.isTimestampWaitSupportedForQueues();
    enabled &= !hwInfoConfig.isDcFlushAllowed();

    switch (DebugManager.flags.EnableTimestampWaitForQueues.get()) {
    case 0:
        enabled = false;
        break;
    case 1:
        enabled = getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled();
        break;
    case 2:
        enabled = getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled();
        break;
    case 3:
        enabled = getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled();
        break;
    case 4:
        enabled = true;
        break;
    }

    return enabled;
}

WaitStatus CommandQueue::waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList) {
    if (blockedQueue) {
        while (isQueueBlocked()) {
        }
    }

    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};
    for (CopyEngineState &state : this->bcsStates) {
        if (state.isValid()) {
            activeBcsStates.push_back(state);
        }
    }

    auto waitedOnTimestamps = waitForTimestamps(activeBcsStates, taskCount);

    TimestampPacketContainer nodesToRelease;
    if (deferredTimestampPackets) {
        deferredTimestampPackets->swapNodes(nodesToRelease);
    }

    const auto waitStatus = waitUntilComplete(taskCount, activeBcsStates, flushStamp->peekStamp(), false, cleanTemporaryAllocationsList, waitedOnTimestamps);

    if (printfHandler) {
        if (!printfHandler->printEnqueueOutput()) {
            return WaitStatus::GpuHang;
        }
    }

    return waitStatus;
}

void CommandQueue::setupBarrierTimestampForBcsEngines(aub_stream::EngineType engineType, TimestampPacketDependencies &timestampPacketDependencies) {
    if (!getGpgpuCommandStreamReceiver().isStallingCommandsOnNextFlushRequired()) {
        return;
    }

    // Ensure we have exactly 1 barrier node.
    if (timestampPacketDependencies.barrierNodes.peekNodes().empty()) {
        timestampPacketDependencies.barrierNodes.add(getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    }

    if (isOOQEnabled()) {
        // Barrier node will be signalled on gpgpuCsr. Save it for later use on blitters.
        for (auto currentBcsIndex = 0u; currentBcsIndex < bcsTimestampPacketContainers.size(); currentBcsIndex++) {
            const auto currentBcsEngineType = EngineHelpers::mapBcsIndexToEngineType(currentBcsIndex, true);
            if (currentBcsEngineType == engineType) {
                // Node is already added to barrierNodes for this engine, no need to save it.
                continue;
            }

            // Save latest timestamp (override previous, if any).
            if (!bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.peekNodes().empty()) {
                for (auto &node : bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.peekNodes()) {
                    this->assignDataToOverwrittenBcsNode(node);
                }
            }
            TimestampPacketContainer newContainer{};
            newContainer.assignAndIncrementNodesRefCounts(timestampPacketDependencies.barrierNodes);
            bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.swapNodes(newContainer);
        }
    }
}

void CommandQueue::processBarrierTimestampForBcsEngine(aub_stream::EngineType bcsEngineType, TimestampPacketDependencies &blitDependencies) {
    BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[EngineHelpers::getBcsIndex(bcsEngineType)];
    bcsContainers.lastBarrierToWaitFor.moveNodesToNewContainer(blitDependencies.barrierNodes);
}

void CommandQueue::setLastBcsPacket(aub_stream::EngineType bcsEngineType) {
    if (isOOQEnabled()) {
        TimestampPacketContainer dummyContainer{};
        dummyContainer.assignAndIncrementNodesRefCounts(*this->timestampPacketContainer);

        BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[EngineHelpers::getBcsIndex(bcsEngineType)];
        bcsContainers.lastSignalledPacket.swapNodes(dummyContainer);
    }
}

void CommandQueue::fillCsrDependenciesWithLastBcsPackets(CsrDependencies &csrDeps) {
    for (BcsTimestampPacketContainers &bcsContainers : bcsTimestampPacketContainers) {
        if (bcsContainers.lastSignalledPacket.peekNodes().empty()) {
            continue;
        }
        csrDeps.timestampPacketContainer.push_back(&bcsContainers.lastSignalledPacket);
    }
}

void CommandQueue::clearLastBcsPackets() {
    for (BcsTimestampPacketContainers &bcsContainers : bcsTimestampPacketContainers) {
        bcsContainers.lastSignalledPacket.moveNodesToNewContainer(*deferredTimestampPackets);
    }
}

} // namespace NEO

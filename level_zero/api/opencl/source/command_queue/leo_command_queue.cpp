/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/performance_counters.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_get_info_status_mapper.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"
#include "level_zero/core/source/cmdqueue/internal_queue_throttle_ext.h"

#include <limits>

namespace NEO {
namespace LEO {

namespace {
bool isAllocationDisplayable(const NEO::GraphicsAllocation &graphicsAllocation) {
    auto gmm = graphicsAllocation.getDefaultGmm();
    return (gmm != nullptr) && (gmm->gmmResourceInfo != nullptr) && gmm->gmmResourceInfo->isDisplayable();
}
} // namespace

CommandQueue::CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties, ze_command_list_handle_t cmdListHandle) : clDevice(device), context(context), externalHandle(true), cmdListHandle(cmdListHandle) {
    context->incRefInternal();

    this->storeProperties(properties);
}

CommandQueue::CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties) : clDevice(device), context(context) {
    context->incRefInternal();

    this->storeProperties(properties);

    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(device);
    auto contextHandle = NEO::LEO::castToObject<NEO::LEO::Context>(context)->getL0ContextHandle();

    auto l0Priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    bool foundPriority = false;
    auto cmdQueuePriority = this->getCmdQueueProperties<cl_queue_priority_khr>(properties, CL_QUEUE_PRIORITY_KHR, &foundPriority);
    if (foundPriority) {
        switch (cmdQueuePriority) {
        case CL_QUEUE_PRIORITY_HIGH_KHR:
            l0Priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
            break;
        case CL_QUEUE_PRIORITY_LOW_KHR:
            l0Priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
            break;
        case CL_QUEUE_PRIORITY_MED_KHR:
        default:
            break;
        }
    }

    ze_command_queue_flags_t l0CmdListFlags = ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT;
    const bool outOfOrder = getCmdQueueProperties<cl_command_queue_properties>(properties) & static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    if (!outOfOrder) {
        l0CmdListFlags |= ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
    }

    uint32_t l0QueueOrdinal = 0;
    uint32_t l0QueueIndex = 0;
    bool foundQueueFamily = false;
    bool foundQueueIndex = false;
    const auto queueFamilyIndex = this->getCmdQueueProperties<cl_uint>(properties, CL_QUEUE_FAMILY_INTEL, &foundQueueFamily);
    const auto queueIndex = this->getCmdQueueProperties<cl_uint>(properties, CL_QUEUE_INDEX_INTEL, &foundQueueIndex);
    if (foundQueueFamily && foundQueueIndex) {
        l0QueueOrdinal = queueFamilyIndex;
        l0QueueIndex = queueIndex;
    }

    ze_command_queue_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                           nullptr,
                                           l0QueueOrdinal,
                                           l0QueueIndex,
                                           l0CmdListFlags,
                                           ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                           l0Priority};

    L0::ze_queue_throttle_ext_desc_t throttleExtDesc;
    bool foundThrottle = false;
    auto clThrottle = this->getCmdQueueProperties<cl_queue_throttle_khr>(properties, CL_QUEUE_THROTTLE_KHR, &foundThrottle);
    if (foundThrottle) {
        if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_LOW_KHR)) {
            throttleExtDesc.throttle = NEO::QueueThrottle::LOW;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_MED_KHR)) {
            throttleExtDesc.throttle = NEO::QueueThrottle::MEDIUM;
        } else if (clThrottle & static_cast<cl_queue_throttle_khr>(CL_QUEUE_THROTTLE_HIGH_KHR)) {
            throttleExtDesc.throttle = NEO::QueueThrottle::HIGH;
        }
        cmdListDesc.pNext = &throttleExtDesc;
    }

    zeCommandListCreateImmediate(contextHandle, deviceHandle, &cmdListDesc, &this->cmdListHandle);
}

CommandQueue::~CommandQueue() {
    if (this->hasDependencies()) {
        this->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }
    if (perfCountersEnabled) {
        getPerfCounters()->shutdown();
    }
    if (!externalHandle) {
        zeCommandListDestroy(this->cmdListHandle);
    }
    context->decRefInternal();
}

bool CommandQueue::setPerfCountersEnabled() {
    DEBUG_BREAK_IF(clDevice == nullptr);

    auto perfCounters = getPerfCounters();
    if (!perfCounters) {
        return false;
    }

    auto csr = getL0Object()->getCsr(false);
    bool isCcsEngine = csr ? NEO::EngineHelpers::isCcs(csr->getOsContext().getEngineType()) : false;

    perfCountersEnabled = perfCounters->enable(isCcsEngine);

    if (!perfCountersEnabled) {
        perfCounters->shutdown();
    }

    return perfCountersEnabled;
}

PerformanceCounters *CommandQueue::getPerfCounters() {
    return clDevice->getDevice().getPerformanceCounters();
}

cl_int CommandQueue::getCmdQInfo(cl_device_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_SUCCESS;
    GetInfoHelper getInfoHelper(paramValue, paramValueSize, paramValueSizeRet);

    switch (paramName) {
    case CL_QUEUE_CONTEXT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_context>(this->context));
        break;
    case CL_QUEUE_DEVICE: {
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_device_id>(this->clDevice));
        break;
    }
    case CL_QUEUE_REFERENCE_COUNT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_int>(this->getReference()));
        break;
    case CL_QUEUE_PROPERTIES:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_command_queue_properties>(this->getCommandQueueProperties()));
        break;
    case CL_QUEUE_DEVICE_DEFAULT:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_command_queue>(nullptr));
        break;
    case CL_QUEUE_SIZE:
        retVal = CL_INVALID_COMMAND_QUEUE;
        break;
    case CL_QUEUE_PROPERTIES_ARRAY: {
        auto source = this->queueProperties.data();
        auto sourceSize = this->queueProperties.size() * sizeof(cl_queue_properties);
        auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, source, sourceSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
        GetInfo::setParamValueReturnSize(paramValueSizeRet, sourceSize, getInfoStatus);
        break;
    }
    case CL_QUEUE_FAMILY_INTEL:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_uint>(this->getQueueFamilyIndex()));
        break;
    case CL_QUEUE_INDEX_INTEL:
        retVal = changeGetInfoStatusToCLResultType(getInfoHelper.set<cl_uint>(this->getQueueIndexWithinFamily()));
        break;
    case CL_L0_IMMEDIATE_CMD_LIST_HANDLE: {
        auto source = &this->cmdListHandle;
        auto sourceSize = sizeof(this->cmdListHandle);
        auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, source, sourceSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
        GetInfo::setParamValueReturnSize(paramValueSizeRet, sourceSize, getInfoStatus);
        break;
    }
    default:
        retVal = CL_INVALID_VALUE;
        break;
    }

    return retVal;
}

void CommandQueue::storeProperties(const cl_queue_properties *properties) {
    if (properties != nullptr) {
        while (*properties != 0) {
            this->queueProperties.push_back(*properties);
            ++properties;
            this->queueProperties.push_back(*properties);
            ++properties;
        }
        this->queueProperties.push_back(0u);
    }
}

void CommandQueue::acquireHelper(void *userData) {
    auto sharingUserData = static_cast<CommandQueue::SharingUserData *>(userData);

    for (auto object : sharingUserData->memObjects) {
        auto memObject = castToObject<MemObj>(object);
        memObject->peekSharingHandler()->acquire(memObject, sharingUserData->cmdQ->clDevice->getDevice().getRootDeviceIndex());
    }

    delete sharingUserData;
}

void CommandQueue::releaseHelper(void *userData) {
    auto sharingUserData = static_cast<CommandQueue::SharingUserData *>(userData);

    for (auto object : sharingUserData->memObjects) {
        auto memObject = castToObject<MemObj>(object);
        memObject->peekSharingHandler()->release(memObject, sharingUserData->cmdQ->clDevice->getDevice().getRootDeviceIndex());
    }

    delete sharingUserData;
}

bool CommandQueue::isInPlaceSharingAcquireReleaseEnabled(cl_uint cmdType) {
    auto inPlaceMode = debugManager.flags.LeoInPlaceSharingAcquireRelease.get();
    if (inPlaceMode != -1) {
        return inPlaceMode != 0;
    }

    switch (cmdType) {
    case CL_COMMAND_ACQUIRE_GL_OBJECTS:
    case CL_COMMAND_RELEASE_GL_OBJECTS:
#if defined(_WIN32)
    case CL_COMMAND_ACQUIRE_D3D10_OBJECTS_KHR:
    case CL_COMMAND_RELEASE_D3D10_OBJECTS_KHR:
    case CL_COMMAND_ACQUIRE_D3D11_OBJECTS_KHR:
    case CL_COMMAND_RELEASE_D3D11_OBJECTS_KHR:
    case CL_COMMAND_ACQUIRE_DX9_MEDIA_SURFACES_KHR:
    case CL_COMMAND_RELEASE_DX9_MEDIA_SURFACES_KHR:
    case CL_COMMAND_ACQUIRE_DX9_OBJECTS_INTEL:
    case CL_COMMAND_RELEASE_DX9_OBJECTS_INTEL:
#endif
        return true;
    default:
        return false;
    }
}

cl_int CommandQueue::acquireSharedObjectsInPlace(cl_uint numObjects,
                                                 const cl_mem *memObjects,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *oclEvent,
                                                 cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    const auto rootDeviceIndex = this->clDevice->getDevice().getRootDeviceIndex();
    for (cl_uint object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto result = memObject->peekSharingHandler()->acquire(memObject, rootDeviceIndex);
        if (result != CL_SUCCESS) {
            return result;
        }
        ++memObject->acquireCount;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, oclEvent, cmdType, this);

    auto lock = this->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(this->cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CommandQueue::releaseSharedObjectsInPlace(cl_uint numObjects,
                                                 const cl_mem *memObjects,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *oclEvent,
                                                 cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (cl_uint i = 0; i < numEventsInWaitList; i++) {
        auto eventObject = castToObject<Event>(eventWaitList[i]);
        if (eventObject == nullptr) {
            return CL_INVALID_EVENT_WAIT_LIST;
        }
        if (eventObject->isUserEvent()) {
            continue;
        }

        auto result = eventObject->wait();
        if (result != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(result);
        }
    }

    if (!this->isOutOfOrder()) {
        auto result = this->hostSynchronize(std::numeric_limits<uint64_t>::max());
        if (result != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(result);
        }
    }

    bool isImageReleased = false;
    bool isDisplayableReleased = false;
    const auto rootDeviceIndex = this->clDevice->getDevice().getRootDeviceIndex();
    for (cl_uint object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        auto graphicsAllocation = memObject->getGraphicsAllocation(rootDeviceIndex);
        if (graphicsAllocation != nullptr) {
            isImageReleased |= graphicsAllocation->getAllocationType() == NEO::AllocationType::sharedImage;
            isDisplayableReleased |= isAllocationDisplayable(*graphicsAllocation);
        }

        memObject->peekSharingHandler()->release(memObject, rootDeviceIndex);
        DEBUG_BREAK_IF(memObject->acquireCount == 0);
        --memObject->acquireCount;
    }

    if (isImageReleased || isDisplayableReleased) {
        auto csr = this->getL0Object()->getCsr(false);
        if (csr != nullptr && csr->isDirectSubmissionEnabled()) {
            csr->sendRenderStateCacheFlush();

            if (isDisplayableReleased) {
                auto result = this->hostSynchronize(std::numeric_limits<uint64_t>::max());
                if (result != ZE_RESULT_SUCCESS) {
                    return L0ToClResultMapper(result);
                }
            }
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, oclEvent, cmdType, this);

    auto lock = this->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(this->cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CommandQueue::enqueueAcquireSharedObjects(cl_uint numObjects,
                                                 const cl_mem *memObjects,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *oclEvent,
                                                 cl_uint cmdType) {
    if (isInPlaceSharingAcquireReleaseEnabled(cmdType)) {
        return this->acquireSharedObjectsInPlace(numObjects, memObjects, numEventsInWaitList, eventWaitList, oclEvent, cmdType);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, oclEvent, cmdType, this);

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        ++memObject->acquireCount;
    }

    auto sharingUserData = new CommandQueue::SharingUserData{this, numObjects, memObjects};

    auto lock = this->takeOwnership();
    return L0ToClResultMapper(L0::zeCommandListAppendHostFunction(this->cmdListHandle,
                                                                  reinterpret_cast<ze_host_function_callback_t>(CommandQueue::acquireHelper),
                                                                  sharingUserData,
                                                                  nullptr,
                                                                  hSignalEvent,
                                                                  waitEvents.size(),
                                                                  waitEvents.data()));
}

cl_int CommandQueue::enqueueReleaseSharedObjects(cl_uint numObjects,
                                                 const cl_mem *memObjects,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *oclEvent,
                                                 cl_uint cmdType) {
    if (isInPlaceSharingAcquireReleaseEnabled(cmdType)) {
        return this->releaseSharedObjectsInPlace(numObjects, memObjects, numEventsInWaitList, eventWaitList, oclEvent, cmdType);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, oclEvent, cmdType, this);

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        --memObject->acquireCount;
    }

    auto sharingUserData = new CommandQueue::SharingUserData{this, numObjects, memObjects};

    auto lock = this->takeOwnership();
    return L0ToClResultMapper(L0::zeCommandListAppendHostFunction(this->cmdListHandle,
                                                                  reinterpret_cast<ze_host_function_callback_t>(CommandQueue::releaseHelper),
                                                                  sharingUserData,
                                                                  nullptr,
                                                                  hSignalEvent,
                                                                  waitEvents.size(),
                                                                  waitEvents.data()));
}

void CommandQueue::storeDependencies(cl_uint numEvents, const cl_event *eventWaitList) {
    if (numEvents == 0) {
        return;
    }

    auto lock = this->takeOwnership();
    for (cl_uint i = 0; i < numEvents; ++i) {
        auto *pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventWaitList[i]);
        pEvent->incRefInternal();
        this->dependencyEvents.push_back(pEvent);
    }
}

void CommandQueue::clearDependencies() {
    auto lock = this->takeOwnership();
    for (auto *pEvent : this->dependencyEvents) {
        pEvent->decRefInternal();
    }
    this->dependencyEvents.clear();
}

bool CommandQueue::hasDependencies() {
    auto lock = this->takeOwnership();
    return !this->dependencyEvents.empty();
}

ze_result_t CommandQueue::hostSynchronize(uint64_t timeout) {
    auto result = zeCommandListHostSynchronize(this->cmdListHandle, timeout);
    if (result == ZE_RESULT_SUCCESS) {
        this->clearDependencies();
    }
    return result;
}

} // namespace LEO
} // namespace NEO

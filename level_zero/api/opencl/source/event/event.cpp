/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/event/event.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/performance_counters.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/api/internal/l0_event.h"
#include "level_zero/api/opencl/source/helpers/get_info_status_mapper.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/driver_experimental/zex_event.h"

namespace NEO {
namespace LEO {

EventHandleSpan::EventHandleSpan(cl_uint numEvents, const cl_event *eventWaitList) noexcept {
    if (eventWaitList && numEvents > 0) {
        count = numEvents;
        ze_event_handle_t *ptr;
        if (numEvents <= maxInlineWaitEvents) {
            ptr = std::get<InlineStorage>(storage).data();
        } else {
            storage = HeapStorage(numEvents);
            ptr = std::get<HeapStorage>(storage).data();
        }
        for (cl_uint i = 0; i < numEvents; ++i) {
            ptr[i] = NEO::LEO::castToObject<NEO::LEO::Event>(eventWaitList[i])->getL0Handle();
        }
    }
}

Event::Event(cl_command_type commandType, NEO::LEO::CommandQueue *commandQueue) : commandType(commandType), oclObj(commandQueue) {
    if (!commandQueue) {
        UNRECOVERABLE_IF(true);
    }
    commandQueue->incRefInternal();

    if (commandQueue->isProfilingEnabled()) {
        this->setQueueTimeStamp();
    }

    ze_event_handle_t hSignalEvent{};
    auto l0CmdList = commandQueue->getL0Object();
    auto deviceHandle = l0CmdList->getDevice();
    auto contextHandle = l0CmdList->getCmdListContext();

    zex_counter_based_event_desc_t eventDesc{ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC, nullptr, ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_DEVICE};
    if (commandQueue->isProfilingEnabled()) {
        eventDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
    }

    L0::zexCounterBasedEventCreate2(contextHandle, deviceHandle, &eventDesc, &hSignalEvent);

    this->eventHandle = hSignalEvent;
    this->perfCountersEnabled = commandQueue->isPerfCountersEnabled();
}

Event::Event(NEO::LEO::Context *context) : commandType(CL_COMMAND_USER), oclObj(context) {
    if (!context) {
        UNRECOVERABLE_IF(true);
    }
    context->incRefInternal();
    this->eventHandle = context->createUserEvent();
}

Event::~Event() {
    this->arbEvent.reset(nullptr);

    if (perfCounterNode != nullptr) {
        UNRECOVERABLE_IF(!std::holds_alternative<CommandQueue *>(oclObj));
        auto cmdQ = std::get<CommandQueue *>(oclObj);
        cmdQ->getPerfCounters()->deleteQuery(perfCounterNode->getQueryHandleRef());
        perfCounterNode->getQueryHandleRef() = {};
        perfCounterNode->returnTag();
    }

    zeEventDestroy(this->eventHandle);

    std::visit([](auto &baseObject) { baseObject->decRefInternal(); }, oclObj);
}

cl_int Event::getProfilingInfo(cl_profiling_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    if (this->isUserEvent()) {
        return CL_PROFILING_INFO_NOT_AVAILABLE;
    }

    const void *src = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;
    uint64_t timestamp = 0;

    ze_kernel_timestamp_result_t ts{};
    zeEventQueryKernelTimestamp(eventHandle, &ts);

    auto device = getL0Object()->getDevice();
    auto resolution = device->getNEODevice()->getOSTime()->getDynamicDeviceTimerResolution();

    switch (paramName) {
    case CL_PROFILING_COMMAND_QUEUED:
        timestamp = device->getGfxCoreHelper().getGpuTimeStampInNS(queueTimeStamp.gpuTimeStamp, resolution);
        break;

    case CL_PROFILING_COMMAND_SUBMIT:
        timestamp = device->getGfxCoreHelper().getGpuTimeStampInNS(submitTimeStamp.gpuTimeStamp, resolution);
        break;

    case CL_PROFILING_COMMAND_START:
        timestamp = device->getGfxCoreHelper().getGpuTimeStampInNS(ts.global.kernelStart, resolution);
        break;

    case CL_PROFILING_COMMAND_END:
    case CL_PROFILING_COMMAND_COMPLETE:
        timestamp = device->getGfxCoreHelper().getGpuTimeStampInNS(ts.global.kernelEnd, resolution);
        break;

    case CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL:
        if (!perfCountersEnabled) {
            return CL_INVALID_VALUE;
        }
        {
            auto cmdQ = getCommandQueue();
            if (!cmdQ->getPerfCounters()->getApiReport(perfCounterNode,
                                                       paramValueSize,
                                                       paramValue,
                                                       paramValueSizeRet,
                                                       queryAndUpdateEventStatus() == CL_COMPLETE)) {
                return CL_PROFILING_INFO_NOT_AVAILABLE;
            }
            return CL_SUCCESS;
        }
    default:
        return CL_INVALID_VALUE;
    }

    src = &timestamp;
    srcSize = sizeof(srcSize);

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, src, srcSize);
    auto retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
}

cl_int Event::getEventInfo(cl_event_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal = CL_INVALID_VALUE;
    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);

    auto cmdQ = this->isUserEvent() ? nullptr : std::get<CommandQueue *>(oclObj);
    auto context = this->getContext();

    switch (paramName) {
    default:
        return retVal;
    case CL_L0_EVENT_HANDLE: {
        auto source = &this->eventHandle;
        auto sourceSize = sizeof(this->eventHandle);
        auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, source, sourceSize);
        retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
        GetInfo::setParamValueReturnSize(paramValueSizeRet, sourceSize, getInfoStatus);
        return retVal;
    }
    case CL_EVENT_COMMAND_QUEUE:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_command_queue>(cmdQ));
        return retVal;
    case CL_EVENT_CONTEXT:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_context>(context));
        return retVal;
    case CL_EVENT_COMMAND_TYPE:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_command_type>(this->commandType));
        return retVal;
    case CL_EVENT_COMMAND_EXECUTION_STATUS:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(this->queryAndUpdateEventStatus()));
        return retVal;
    case CL_EVENT_REFERENCE_COUNT:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_uint>(this->getReference()));
        return retVal;
    }
}

ze_result_t Event::wait() {
    return zeEventHostSynchronize(this->eventHandle, std::numeric_limits<uint64_t>::max());
}

ze_result_t Event::signal() {
    return zeEventHostSignal(this->eventHandle);
}

cl_int Event::queryAndUpdateEventStatus() {
    if (zeEventQueryStatus(this->eventHandle) == ZE_RESULT_SUCCESS) {
        this->eventStatus = CL_COMPLETE;
    }
    return this->eventStatus;
}

void Event::setQueueTimeStamp() {
    this->getCommandQueue()->getDevice()->getDevice().getOSTime()->getCpuTime(&queueTimeStamp.cpuTimeInNs);
}

void Event::setSubmitTimeStamp() {
    auto device = getL0Object()->getDevice()->getNEODevice();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto resolution = device->getDeviceInfo().profilingTimerResolution;

    device->getOSTime()->getCpuTime(&this->submitTimeStamp.cpuTimeInNs);
    TimeStampData submitCpuGpuTime{};
    device->getOSTime()->getGpuCpuTime(&submitCpuGpuTime);
    this->submitTimeStamp.gpuTimeInNs = gfxCoreHelper.getGpuTimeStampInNS(submitCpuGpuTime.gpuTimeStamp, resolution);
    this->submitTimeStamp.gpuTimeStamp = submitCpuGpuTime.gpuTimeStamp;

    setupRelativeProfilingInfo(queueTimeStamp);
}

void Event::setupRelativeProfilingInfo(ProfilingInfo &profilingInfo) {
    auto resolution = getL0Object()->getDevice()->getNEODevice()->getDeviceInfo().profilingTimerResolution;

    if (profilingInfo.cpuTimeInNs > submitTimeStamp.cpuTimeInNs) {
        auto timeDiff = profilingInfo.cpuTimeInNs - submitTimeStamp.cpuTimeInNs;
        auto gpuTicksDiff = static_cast<uint64_t>(timeDiff / resolution);
        profilingInfo.gpuTimeInNs = submitTimeStamp.gpuTimeInNs + timeDiff;
        profilingInfo.gpuTimeStamp = submitTimeStamp.gpuTimeStamp + std::max<uint64_t>(gpuTicksDiff, 1ul);
    } else if (profilingInfo.cpuTimeInNs < submitTimeStamp.cpuTimeInNs) {
        auto timeDiff = submitTimeStamp.cpuTimeInNs - profilingInfo.cpuTimeInNs;
        auto gpuTicksDiff = static_cast<uint64_t>(timeDiff / resolution);
        profilingInfo.gpuTimeInNs = submitTimeStamp.gpuTimeInNs - timeDiff;
        profilingInfo.gpuTimeStamp = submitTimeStamp.gpuTimeStamp - std::max<uint64_t>(gpuTicksDiff, 1ul);
    } else {
        profilingInfo.gpuTimeInNs = submitTimeStamp.gpuTimeInNs;
        profilingInfo.gpuTimeStamp = submitTimeStamp.gpuTimeStamp;
    }
}

std::pair<EventHandleSpan, ze_event_handle_t> Event::setupEvents(cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                                                 cl_event *event, cl_command_type commandType,
                                                                 NEO::LEO::CommandQueue *commandQueue) {
    EventHandleSpan waitEvents{numEventsInWaitList, eventWaitList};
    ze_event_handle_t signalEvent = nullptr;

    if (event) {
        auto e = std::make_unique<NEO::LEO::Event>(commandType, commandQueue);
        signalEvent = e->eventHandle;

        if (commandQueue->isProfilingEnabled()) {
            e->setSubmitTimeStamp();
        }

        *event = e.release();
    }

    return {std::move(waitEvents), signalEvent};
}

TagNodeBase *Event::getHwPerfCounterNode() {
    if (!perfCounterNode) {
        auto cmdQ = getCommandQueue();
        auto perfCounters = cmdQ->getPerfCounters();
        if (perfCounters) {
            const uint32_t gpuReportSize = HwPerfCounter::getSize(*perfCounters);
            auto csr = cmdQ->getL0Object()->getCsr(false);
            if (csr) {
                perfCounterNode = csr->getEventPerfCountAllocator(gpuReportSize)->getTag();
            }
        }
    }
    return perfCounterNode;
}

} // namespace LEO
} // namespace NEO

/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/event/event.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/range.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/async_events_handler.h"
#include "opencl/source/event/event_tracker.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/mem_obj.h"

#include <algorithm>

namespace NEO {

Event::Event(
    Context *ctx,
    CommandQueue *cmdQueue,
    cl_command_type cmdType,
    uint32_t taskLevel,
    uint32_t taskCount)
    : taskLevel(taskLevel),
      currentCmdQVirtualEvent(false),
      cmdToSubmit(nullptr),
      submittedCmd(nullptr),
      ctx(ctx),
      cmdQueue(cmdQueue),
      cmdType(cmdType),
      dataCalculated(false),
      taskCount(taskCount) {
    if (NEO::DebugManager.flags.EventsTrackerEnable.get()) {
        EventsTracker::getEventsTracker().notifyCreation(this);
    }
    parentCount = 0;
    executionStatus = CL_QUEUED;
    flushStamp.reset(new FlushStampTracker(true));

    DBG_LOG(EventsDebugEnable, "Event()", this);

    // Event can live longer than command queue that created it,
    // hence command queue refCount must be incremented
    // non-null command queue is only passed when Base Event object is created
    // any other Event types must increment refcount when setting command queue
    if (cmdQueue != nullptr) {
        cmdQueue->incRefInternal();
    }

    if ((this->ctx == nullptr) && (cmdQueue != nullptr)) {
        this->ctx = &cmdQueue->getContext();
        if (cmdQueue->getTimestampPacketContainer()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
        }
    }

    if (this->ctx != nullptr) {
        this->ctx->incRefInternal();
    }

    queueTimeStamp = {0, 0};
    submitTimeStamp = {0, 0};
    startTimeStamp = 0;
    endTimeStamp = 0;
    completeTimeStamp = 0;

    profilingEnabled = !isUserEvent() &&
                       (cmdQueue ? cmdQueue->getCommandQueueProperties() & CL_QUEUE_PROFILING_ENABLE : false);
    profilingCpuPath = ((cmdType == CL_COMMAND_MAP_BUFFER) || (cmdType == CL_COMMAND_MAP_IMAGE)) && profilingEnabled;

    perfCountersEnabled = cmdQueue ? cmdQueue->isPerfCountersEnabled() : false;
}

Event::Event(
    CommandQueue *cmdQueue,
    cl_command_type cmdType,
    uint32_t taskLevel,
    uint32_t taskCount)
    : Event(nullptr, cmdQueue, cmdType, taskLevel, taskCount) {
}

Event::~Event() {
    if (NEO::DebugManager.flags.EventsTrackerEnable.get()) {
        EventsTracker::getEventsTracker().notifyDestruction(this);
    }

    DBG_LOG(EventsDebugEnable, "~Event()", this);
    // no commands should be registred
    DEBUG_BREAK_IF(this->cmdToSubmit.load());

    submitCommand(true);

    int32_t lastStatus = executionStatus;
    if (isStatusCompleted(lastStatus) == false) {
        transitionExecutionStatus(-1);
        DEBUG_BREAK_IF(peekHasCallbacks() || peekHasChildEvents());
    }

    // Note from OCL spec:
    //    "All callbacks registered for an event object must be called.
    //     All enqueued callbacks shall be called before the event object is destroyed."
    if (peekHasCallbacks()) {
        executeCallbacks(lastStatus);
    }

    {
        // clean-up submitted command if needed
        std::unique_ptr<Command> submittedCommand(submittedCmd.exchange(nullptr));
    }

    if (cmdQueue != nullptr) {
        if (timeStampNode != nullptr) {
            timeStampNode->returnTag();
        }
        if (perfCounterNode != nullptr) {
            cmdQueue->getPerfCounters()->deleteQuery(perfCounterNode->getQueryHandleRef());
            perfCounterNode->getQueryHandleRef() = {};
            perfCounterNode->returnTag();
        }
        cmdQueue->decRefInternal();
    }

    if (ctx != nullptr) {
        ctx->decRefInternal();
    }

    // in case event did not unblock child events before
    unblockEventsBlockedByThis(executionStatus);
}

cl_int Event::getEventProfilingInfo(cl_profiling_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    cl_int retVal;
    const void *src = nullptr;
    size_t srcSize = GetInfo::invalidSourceSize;

    // CL_PROFILING_INFO_NOT_AVAILABLE if event refers to the clEnqueueSVMFree command
    if (isUserEvent() != CL_FALSE ||         // or is a user event object.
        !updateStatusAndCheckCompletion() || // if the execution status of the command identified by event is not CL_COMPLETE
        !profilingEnabled)                   // the CL_QUEUE_PROFILING_ENABLE flag is not set for the command-queue,
    {
        return CL_PROFILING_INFO_NOT_AVAILABLE;
    }

    uint64_t timestamp = 0u;

    // if paramValue is NULL, it is ignored
    switch (paramName) {
    case CL_PROFILING_COMMAND_QUEUED:
        timestamp = getTimeInNSFromTimestampData(queueTimeStamp);
        src = &timestamp;
        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_SUBMIT:
        calculateSubmitTimestampData();
        timestamp = getTimeInNSFromTimestampData(submitTimeStamp);
        src = &timestamp;
        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_START:
        calcProfilingData();
        src = &startTimeStamp;
        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_END:
        calcProfilingData();
        src = &endTimeStamp;
        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_COMPLETE:
        calcProfilingData();
        src = &completeTimeStamp;
        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_PERFCOUNTERS_INTEL:
        if (!perfCountersEnabled) {
            return CL_INVALID_VALUE;
        }

        if (!cmdQueue->getPerfCounters()->getApiReport(perfCounterNode,
                                                       paramValueSize,
                                                       paramValue,
                                                       paramValueSizeRet,
                                                       updateStatusAndCheckCompletion())) {
            return CL_PROFILING_INFO_NOT_AVAILABLE;
        }
        return CL_SUCCESS;
    default:
        return CL_INVALID_VALUE;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, src, srcSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcSize, getInfoStatus);

    return retVal;
} // namespace NEO

void Event::setupBcs(aub_stream::EngineType bcsEngineType) {
    DEBUG_BREAK_IF(!EngineHelpers::isBcs(bcsEngineType));
    this->bcsState.engineType = bcsEngineType;
}

uint32_t Event::peekBcsTaskCountFromCommandQueue() {
    if (bcsState.isValid()) {
        return this->cmdQueue->peekBcsTaskCount(bcsState.engineType);
    } else {
        return 0u;
    }
}

uint32_t Event::getCompletionStamp() const {
    return this->taskCount;
}

void Event::updateCompletionStamp(uint32_t gpgpuTaskCount, uint32_t bcsTaskCount, uint32_t tasklevel, FlushStamp flushStamp) {
    this->taskCount = gpgpuTaskCount;
    this->bcsState.taskCount = bcsTaskCount;
    this->taskLevel = tasklevel;
    this->flushStamp->setStamp(flushStamp);
}

cl_ulong Event::getDelta(cl_ulong startTime,
                         cl_ulong endTime) {

    auto &hwInfo = cmdQueue->getDevice().getHardwareInfo();

    cl_ulong max = maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits);
    cl_ulong delta = 0;

    startTime &= max;
    endTime &= max;

    if (startTime > endTime) {
        delta = max - startTime;
        delta += endTime;
    } else {
        delta = endTime - startTime;
    }

    return delta;
}

void Event::calculateSubmitTimestampData() {
    if (DebugManager.flags.EnableDeviceBasedTimestamps.get()) {
        auto &device = cmdQueue->getDevice();
        auto &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
        double resolution = device.getDeviceInfo().profilingTimerResolution;

        int64_t timerDiff = queueTimeStamp.CPUTimeinNS - hwHelper.getGpuTimeStampInNS(queueTimeStamp.GPUTimeStamp, resolution);
        submitTimeStamp.GPUTimeStamp = static_cast<uint64_t>((submitTimeStamp.CPUTimeinNS - timerDiff) / resolution);
    }
}

uint64_t Event::getTimeInNSFromTimestampData(const TimeStampData &timestamp) const {
    if (isCPUProfilingPath()) {
        return timestamp.CPUTimeinNS;
    }

    if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
        return timestamp.GPUTimeStamp;
    }

    if (cmdQueue && DebugManager.flags.EnableDeviceBasedTimestamps.get()) {
        auto &device = cmdQueue->getDevice();
        auto &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
        double resolution = device.getDeviceInfo().profilingTimerResolution;

        return hwHelper.getGpuTimeStampInNS(timestamp.GPUTimeStamp, resolution);
    }

    return timestamp.CPUTimeinNS;
}

bool Event::calcProfilingData() {
    if (!dataCalculated && !profilingCpuPath) {
        if (timestampPacketContainer && timestampPacketContainer->peekNodes().size() > 0) {
            const auto timestamps = timestampPacketContainer->peekNodes();

            if (DebugManager.flags.PrintTimestampPacketContents.get()) {

                for (auto i = 0u; i < timestamps.size(); i++) {
                    std::cout << "Timestamp " << i << ", "
                              << "cmd type: " << this->cmdType << ", ";
                    for (auto j = 0u; j < timestamps[i]->getPacketsUsed(); j++) {
                        std::cout << "packet " << j << ": "
                                  << "global start: " << timestamps[i]->getGlobalStartValue(j) << ", "
                                  << "global end: " << timestamps[i]->getGlobalEndValue(j) << ", "
                                  << "context start: " << timestamps[i]->getContextStartValue(j) << ", "
                                  << "context end: " << timestamps[i]->getContextEndValue(j) << ", "
                                  << "global delta: " << timestamps[i]->getGlobalEndValue(j) - timestamps[i]->getGlobalStartValue(j) << ", "
                                  << "context delta: " << timestamps[i]->getContextEndValue(j) - timestamps[i]->getContextStartValue(j) << std::endl;
                    }
                }
            }

            uint64_t globalStartTS = 0u;
            uint64_t globalEndTS = 0u;
            Event::getBoundaryTimestampValues(timestampPacketContainer.get(), globalStartTS, globalEndTS);

            calculateProfilingDataInternal(globalStartTS, globalEndTS, &globalEndTS, globalStartTS);

        } else if (timeStampNode) {
            if (HwHelper::get(this->cmdQueue->getDevice().getHardwareInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
                calculateProfilingDataInternal(
                    timeStampNode->getGlobalStartValue(0),
                    timeStampNode->getGlobalEndValue(0),
                    &timeStampNode->getGlobalEndRef(),
                    timeStampNode->getGlobalStartValue(0));
            } else {
                calculateProfilingDataInternal(
                    timeStampNode->getContextStartValue(0),
                    timeStampNode->getContextEndValue(0),
                    &timeStampNode->getContextCompleteRef(),
                    timeStampNode->getGlobalStartValue(0));
            }
        }
    }
    return dataCalculated;
}

void Event::calculateProfilingDataInternal(uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS) {
    uint64_t gpuDuration = 0;
    uint64_t cpuDuration = 0;

    uint64_t gpuCompleteDuration = 0;
    uint64_t cpuCompleteDuration = 0;

    auto &device = this->cmdQueue->getDevice();
    auto &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
    auto frequency = device.getDeviceInfo().profilingTimerResolution;
    auto gpuQueueTimeStamp = hwHelper.getGpuTimeStampInNS(queueTimeStamp.GPUTimeStamp, frequency);

    if (DebugManager.flags.EnableDeviceBasedTimestamps.get()) {
        startTimeStamp = static_cast<uint64_t>(globalStartTS * frequency);
        if (startTimeStamp < gpuQueueTimeStamp) {
            startTimeStamp += static_cast<uint64_t>((1ULL << hwHelper.getGlobalTimeStampBits()) * frequency);
        }
    } else {
        int64_t c0 = queueTimeStamp.CPUTimeinNS - gpuQueueTimeStamp;
        startTimeStamp = static_cast<uint64_t>(globalStartTS * frequency) + c0;
        if (startTimeStamp < queueTimeStamp.CPUTimeinNS) {
            c0 += static_cast<uint64_t>((1ULL << (hwHelper.getGlobalTimeStampBits())) * frequency);
            startTimeStamp = static_cast<uint64_t>(globalStartTS * frequency) + c0;
        }
    }

    /* calculation based on equation
       CpuTime = GpuTime * scalar + const( == c0)
       scalar = DeltaCpu( == dCpu) / DeltaGpu( == dGpu)
       to determine the value of the const we can use one pair of values
       const = CpuTimeQueue - GpuTimeQueue * scalar
    */

    // If device enqueue has not updated complete timestamp, assign end timestamp
    gpuDuration = getDelta(contextStartTS, contextEndTS);
    if (*contextCompleteTS == 0) {
        *contextCompleteTS = contextEndTS;
        gpuCompleteDuration = gpuDuration;
    } else {
        gpuCompleteDuration = getDelta(contextStartTS, *contextCompleteTS);
    }
    cpuDuration = static_cast<uint64_t>(gpuDuration * frequency);
    cpuCompleteDuration = static_cast<uint64_t>(gpuCompleteDuration * frequency);

    endTimeStamp = startTimeStamp + cpuDuration;
    completeTimeStamp = startTimeStamp + cpuCompleteDuration;

    if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
        startTimeStamp = contextStartTS;
        endTimeStamp = contextEndTS;
        completeTimeStamp = *contextCompleteTS;
    }

    dataCalculated = true;
}

void Event::getBoundaryTimestampValues(TimestampPacketContainer *timestampContainer, uint64_t &globalStartTS, uint64_t &globalEndTS) {
    const auto timestamps = timestampContainer->peekNodes();

    globalStartTS = timestamps[0]->getGlobalStartValue(0);
    globalEndTS = timestamps[0]->getGlobalEndValue(0);

    for (const auto &timestamp : timestamps) {
        if (!timestamp->isProfilingCapable()) {
            continue;
        }
        for (auto i = 0u; i < timestamp->getPacketsUsed(); ++i) {
            if (globalStartTS > timestamp->getGlobalStartValue(i)) {
                globalStartTS = timestamp->getGlobalStartValue(i);
            }
            if (globalEndTS < timestamp->getGlobalEndValue(i)) {
                globalEndTS = timestamp->getGlobalEndValue(i);
            }
        }
    }
}

inline WaitStatus Event::wait(bool blocking, bool useQuickKmdSleep) {
    while (this->taskCount == CompletionStamp::notReady) {
        if (blocking == false) {
            return WaitStatus::NotReady;
        }
    }

    Range<CopyEngineState> states{&bcsState, bcsState.isValid() ? 1u : 0u};
    const auto waitStatus = cmdQueue->waitUntilComplete(taskCount.load(), states, flushStamp->peekStamp(), useQuickKmdSleep);
    if (waitStatus == WaitStatus::GpuHang) {
        return WaitStatus::GpuHang;
    }
    updateExecutionStatus();

    DEBUG_BREAK_IF(this->taskLevel == CompletionStamp::notReady && this->executionStatus >= 0);

    auto *allocationStorage = cmdQueue->getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    allocationStorage->cleanAllocationList(this->taskCount, TEMPORARY_ALLOCATION);

    return WaitStatus::Ready;
}

void Event::updateExecutionStatus() {
    if (taskLevel == CompletionStamp::notReady) {
        return;
    }

    int32_t statusSnapshot = executionStatus;
    if (isStatusCompleted(statusSnapshot)) {
        executeCallbacks(statusSnapshot);
        return;
    }

    if (peekIsBlocked()) {
        transitionExecutionStatus(CL_QUEUED);
        executeCallbacks(CL_QUEUED);
        return;
    }

    if (statusSnapshot == CL_QUEUED) {
        bool abortBlockedTasks = isStatusCompletedByTermination(statusSnapshot);
        submitCommand(abortBlockedTasks);
        transitionExecutionStatus(CL_SUBMITTED);
        executeCallbacks(CL_SUBMITTED);
        unblockEventsBlockedByThis(CL_SUBMITTED);
        // Note : Intentional fallthrough (no return) to check for CL_COMPLETE
    }

    if ((cmdQueue != nullptr) && this->isCompleted()) {
        transitionExecutionStatus(CL_COMPLETE);
        executeCallbacks(CL_COMPLETE);
        unblockEventsBlockedByThis(CL_COMPLETE);
        auto *allocationStorage = cmdQueue->getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
        allocationStorage->cleanAllocationList(this->taskCount, TEMPORARY_ALLOCATION);
        return;
    }

    transitionExecutionStatus(CL_SUBMITTED);
}

void Event::addChild(Event &childEvent) {
    childEvent.parentCount++;
    childEvent.incRefInternal();
    childEventsToNotify.pushRefFrontOne(childEvent);
    DBG_LOG(EventsDebugEnable, "addChild: Parent event:", this, "child:", &childEvent);
    if (DebugManager.flags.TrackParentEvents.get()) {
        childEvent.parentEvents.push_back(this);
    }
    if (executionStatus == CL_COMPLETE) {
        unblockEventsBlockedByThis(CL_COMPLETE);
    }
}

void Event::unblockEventsBlockedByThis(int32_t transitionStatus) {

    int32_t status = transitionStatus;
    (void)status;
    DEBUG_BREAK_IF(!(isStatusCompleted(status) || (peekIsSubmitted(status))));

    uint32_t taskLevelToPropagate = CompletionStamp::notReady;

    if (isStatusCompletedByTermination(transitionStatus) == false) {
        // if we are event on top of the tree , obtain taskLevel from CSR
        if (taskLevel == CompletionStamp::notReady) {
            this->taskLevel = getTaskLevel(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
            taskLevelToPropagate = this->taskLevel;
        } else {
            taskLevelToPropagate = taskLevel + 1;
        }
    }

    auto childEventRef = childEventsToNotify.detachNodes();
    while (childEventRef != nullptr) {
        auto childEvent = childEventRef->ref;

        childEvent->unblockEventBy(*this, taskLevelToPropagate, transitionStatus);

        childEvent->decRefInternal();
        auto next = childEventRef->next;
        delete childEventRef;
        childEventRef = next;
    }
}

bool Event::setStatus(cl_int status) {
    int32_t prevStatus = executionStatus;

    DBG_LOG(EventsDebugEnable, "setStatus event", this, " new status", status, "previousStatus", prevStatus);

    if (isStatusCompleted(prevStatus)) {
        return false;
    }

    if (status == prevStatus) {
        return false;
    }

    if (peekIsBlocked() && (isStatusCompletedByTermination(status) == false)) {
        return false;
    }

    if ((status == CL_SUBMITTED) || (isStatusCompleted(status))) {
        bool abortBlockedTasks = isStatusCompletedByTermination(status);
        submitCommand(abortBlockedTasks);
    }

    this->incRefInternal();
    transitionExecutionStatus(status);
    if (isStatusCompleted(status) || (status == CL_SUBMITTED)) {
        unblockEventsBlockedByThis(status);
    }
    executeCallbacks(status);
    this->decRefInternal();
    return true;
}

void Event::transitionExecutionStatus(int32_t newExecutionStatus) const {
    int32_t prevStatus = executionStatus;
    DBG_LOG(EventsDebugEnable, "transitionExecutionStatus event", this, " new status", newExecutionStatus, "previousStatus", prevStatus);

    while (prevStatus > newExecutionStatus) {
        executionStatus.compare_exchange_weak(prevStatus, newExecutionStatus);
    }
    if (NEO::DebugManager.flags.EventsTrackerEnable.get()) {
        EventsTracker::getEventsTracker().notifyTransitionedExecutionStatus();
    }
}

void Event::submitCommand(bool abortTasks) {
    std::unique_ptr<Command> cmdToProcess(cmdToSubmit.exchange(nullptr));
    if (cmdToProcess.get() != nullptr) {
        getCommandQueue()->initializeBcsEngine(getCommandQueue()->isSpecial());
        auto lockCSR = getCommandQueue()->getGpgpuCommandStreamReceiver().obtainUniqueOwnership();

        if (this->isProfilingEnabled()) {
            if (timeStampNode) {
                this->cmdQueue->getGpgpuCommandStreamReceiver().makeResident(*timeStampNode->getBaseGraphicsAllocation());
                cmdToProcess->timestamp = timeStampNode;
            }
            if (profilingCpuPath) {
                setSubmitTimeStamp();
                setStartTimeStamp();
            } else {
                this->cmdQueue->getDevice().getOSTime()->getCpuGpuTime(&submitTimeStamp);
            }
            if (perfCountersEnabled && perfCounterNode) {
                this->cmdQueue->getGpgpuCommandStreamReceiver().makeResident(*perfCounterNode->getBaseGraphicsAllocation());
            }
        }

        auto &complStamp = cmdToProcess->submit(taskLevel, abortTasks);
        if (profilingCpuPath && this->isProfilingEnabled()) {
            setEndTimeStamp();
        }

        if (complStamp.taskCount == CompletionStamp::gpuHang) {
            abortExecutionDueToGpuHang();
            return;
        }

        updateTaskCount(complStamp.taskCount, peekBcsTaskCountFromCommandQueue());
        flushStamp->setStamp(complStamp.flushStamp);
        submittedCmd.exchange(cmdToProcess.release());
    } else if (profilingCpuPath && endTimeStamp == 0) {
        setEndTimeStamp();
    }
    if (this->taskCount == CompletionStamp::notReady) {
        if (!this->isUserEvent() && this->eventWithoutCommand) {
            if (this->cmdQueue) {
                auto lockCSR = this->getCommandQueue()->getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
                updateTaskCount(this->cmdQueue->getGpgpuCommandStreamReceiver().peekTaskCount(), peekBcsTaskCountFromCommandQueue());
            }
        }
        // make sure that task count is synchronized for events with kernels
        if (!this->eventWithoutCommand && !abortTasks) {
            this->synchronizeTaskCount();
        }
    }
}

cl_int Event::waitForEvents(cl_uint numEvents,
                            const cl_event *eventList) {
    if (numEvents == 0) {
        return CL_SUCCESS;
    }

    // flush all command queues
    for (const cl_event *it = eventList, *end = eventList + numEvents; it != end; ++it) {
        Event *event = castToObjectOrAbort<Event>(*it);
        if (event->cmdQueue) {
            if (event->taskLevel != CompletionStamp::notReady) {
                event->cmdQueue->flush();
            }
        }
    }

    using WorkerListT = StackVec<cl_event, 64>;
    WorkerListT workerList1(eventList, eventList + numEvents);
    WorkerListT workerList2;
    workerList2.reserve(numEvents);

    // pointers to workerLists - for fast swap operations
    WorkerListT *currentlyPendingEvents = &workerList1;
    WorkerListT *pendingEventsLeft = &workerList2;
    WaitStatus eventWaitStatus = WaitStatus::NotReady;

    while (currentlyPendingEvents->size() > 0) {
        for (auto current = currentlyPendingEvents->begin(), end = currentlyPendingEvents->end(); current != end; ++current) {
            Event *event = castToObjectOrAbort<Event>(*current);
            if (event->peekExecutionStatus() < CL_COMPLETE) {
                return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            }

            eventWaitStatus = event->wait(false, false);
            if (eventWaitStatus == WaitStatus::NotReady) {
                pendingEventsLeft->push_back(event);
            } else if (eventWaitStatus == WaitStatus::GpuHang) {
                setExecutionStatusToAbortedDueToGpuHang(pendingEventsLeft->begin(), pendingEventsLeft->end());
                setExecutionStatusToAbortedDueToGpuHang(current, end);

                return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            }
        }

        std::swap(currentlyPendingEvents, pendingEventsLeft);
        pendingEventsLeft->clear();
    }

    return CL_SUCCESS;
}

inline void Event::setExecutionStatusToAbortedDueToGpuHang(cl_event *first, cl_event *last) {
    std::for_each(first, last, [](cl_event &e) {
        Event *event = castToObjectOrAbort<Event>(e);
        event->abortExecutionDueToGpuHang();
    });
}

bool Event::isCompleted() {
    return cmdQueue->isCompleted(getCompletionStamp(), this->bcsState) || this->areTimestampsCompleted();
}

bool Event::isWaitForTimestampsEnabled() const {
    const auto &hwInfo = cmdQueue->getDevice().getHardwareInfo();
    const auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto enabled = cmdQueue->isTimestampWaitEnabled();
    enabled &= hwHelper.isTimestampWaitSupportedForEvents(hwInfo);

    switch (DebugManager.flags.EnableTimestampWaitForEvents.get()) {
    case 0:
        enabled = false;
        break;
    case 1:
        enabled = cmdQueue->getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled();
        break;
    case 2:
        enabled = cmdQueue->getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled();
        break;
    case 3:
        enabled = cmdQueue->getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled();
        break;
    case 4:
        enabled = true;
        break;
    }

    return enabled;
}

bool Event::areTimestampsCompleted() {
    if (this->timestampPacketContainer.get()) {
        if (this->isWaitForTimestampsEnabled()) {
            for (const auto &timestamp : this->timestampPacketContainer->peekNodes()) {
                for (uint32_t i = 0; i < timestamp->getPacketsUsed(); i++) {
                    this->cmdQueue->getGpgpuCommandStreamReceiver().downloadAllocation(*timestamp->getBaseGraphicsAllocation()->getGraphicsAllocation(this->cmdQueue->getGpgpuCommandStreamReceiver().getRootDeviceIndex()));
                    if (timestamp->getContextEndValue(i) == 1) {
                        return false;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

uint32_t Event::getTaskLevel() {
    return taskLevel;
}

inline void Event::unblockEventBy(Event &event, uint32_t taskLevel, int32_t transitionStatus) {
    int32_t numEventsBlockingThis = --parentCount;
    DEBUG_BREAK_IF(numEventsBlockingThis < 0);

    int32_t blockerStatus = transitionStatus;
    DEBUG_BREAK_IF(!(isStatusCompleted(blockerStatus) || peekIsSubmitted(blockerStatus)));

    if ((numEventsBlockingThis > 0) && (isStatusCompletedByTermination(blockerStatus) == false)) {
        return;
    }
    DBG_LOG(EventsDebugEnable, "Event", this, "is unblocked by", &event);

    if (this->taskLevel == CompletionStamp::notReady) {
        this->taskLevel = std::max(cmdQueue->getGpgpuCommandStreamReceiver().peekTaskLevel(), taskLevel);
    } else {
        this->taskLevel = std::max(this->taskLevel.load(), taskLevel);
    }

    int32_t statusToPropagate = CL_SUBMITTED;
    if (isStatusCompletedByTermination(blockerStatus)) {
        statusToPropagate = blockerStatus;
    }
    setStatus(statusToPropagate);

    // event may be completed after this operation, transtition the state to not block others.
    this->updateExecutionStatus();
}

bool Event::updateStatusAndCheckCompletion() {
    auto currentStatus = updateEventAndReturnCurrentStatus();
    return isStatusCompleted(currentStatus);
}

bool Event::isReadyForSubmission() {
    return taskLevel != CompletionStamp::notReady ? true : false;
}

void Event::addCallback(Callback::ClbFuncT fn, cl_int type, void *data) {
    ECallbackTarget target = translateToCallbackTarget(type);
    if (target == ECallbackTarget::Invalid) {
        DEBUG_BREAK_IF(true);
        return;
    }
    incRefInternal();

    // Note from spec :
    //    "All callbacks registered for an event object must be called.
    //     All enqueued callbacks shall be called before the event object is destroyed."
    // That's why each registered calback increments the internal refcount
    incRefInternal();
    DBG_LOG(EventsDebugEnable, "event", this, "addCallback", "ECallbackTarget", (uint32_t)type);
    callbacks[(uint32_t)target].pushFrontOne(*new Callback(this, fn, type, data));

    // Callback added after event reached its "completed" state
    if (updateStatusAndCheckCompletion()) {
        int32_t status = executionStatus;
        DBG_LOG(EventsDebugEnable, "event", this, "addCallback executing callbacks with status", status);
        executeCallbacks(status);
    }

    if (peekHasCallbacks() && !isUserEvent() && DebugManager.flags.EnableAsyncEventsHandler.get()) {
        ctx->getAsyncEventsHandler().registerEvent(this);
    }

    decRefInternal();
}

void Event::executeCallbacks(int32_t executionStatusIn) {
    int32_t execStatus = executionStatusIn;
    bool terminated = isStatusCompletedByTermination(execStatus);
    ECallbackTarget target;
    if (terminated) {
        target = ECallbackTarget::Completed;
    } else {
        target = translateToCallbackTarget(execStatus);
        if (target == ECallbackTarget::Invalid) {
            DEBUG_BREAK_IF(true);
            return;
        }
    }

    // run through all needed callback targets and execute callbacks
    for (uint32_t i = 0; i <= (uint32_t)target; ++i) {
        auto cb = callbacks[i].detachNodes();
        auto curr = cb;
        while (curr != nullptr) {
            auto next = curr->next;
            if (terminated) {
                curr->overrideCallbackExecutionStatusTarget(execStatus);
            }
            DBG_LOG(EventsDebugEnable, "event", this, "executing callback", "ECallbackTarget", (uint32_t)target);
            curr->execute();
            decRefInternal();
            delete curr;
            curr = next;
        }
    }
}

void Event::tryFlushEvent() {
    // only if event is not completed, completed event has already been flushed
    if (cmdQueue && updateStatusAndCheckCompletion() == false) {
        // flush the command queue only if it is not blocked event
        if (taskLevel != CompletionStamp::notReady) {
            cmdQueue->getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
        }
    }
}

void Event::setQueueTimeStamp() {
    if (this->profilingEnabled && (this->cmdQueue != nullptr)) {
        this->cmdQueue->getDevice().getOSTime()->getCpuTime(&queueTimeStamp.CPUTimeinNS);
    }
}

void Event::setSubmitTimeStamp() {
    if (this->profilingEnabled && (this->cmdQueue != nullptr)) {
        this->cmdQueue->getDevice().getOSTime()->getCpuTime(&submitTimeStamp.CPUTimeinNS);
    }
}

void Event::setStartTimeStamp() {
    if (this->profilingEnabled && (this->cmdQueue != nullptr)) {
        this->cmdQueue->getDevice().getOSTime()->getCpuTime(&startTimeStamp);
    }
}

void Event::setEndTimeStamp() {
    if (this->profilingEnabled && (this->cmdQueue != nullptr)) {
        this->cmdQueue->getDevice().getOSTime()->getCpuTime(&endTimeStamp);
        completeTimeStamp = endTimeStamp;
    }
}

TagNodeBase *Event::getHwTimeStampNode() {
    if (!cmdQueue->getTimestampPacketContainer() && !timeStampNode) {
        timeStampNode = cmdQueue->getGpgpuCommandStreamReceiver().getEventTsAllocator()->getTag();
    }
    return timeStampNode;
}

TagNodeBase *Event::getHwPerfCounterNode() {

    if (!perfCounterNode && cmdQueue->getPerfCounters()) {
        const uint32_t gpuReportSize = HwPerfCounter::getSize(*(cmdQueue->getPerfCounters()));
        perfCounterNode = cmdQueue->getGpgpuCommandStreamReceiver().getEventPerfCountAllocator(gpuReportSize)->getTag();
    }
    return perfCounterNode;
}

void Event::addTimestampPacketNodes(const TimestampPacketContainer &inputTimestampPacketContainer) {
    timestampPacketContainer->assignAndIncrementNodesRefCounts(inputTimestampPacketContainer);
}

TimestampPacketContainer *Event::getTimestampPacketNodes() const { return timestampPacketContainer.get(); }

bool Event::checkUserEventDependencies(cl_uint numEventsInWaitList, const cl_event *eventWaitList) {
    bool userEventsDependencies = false;

    for (uint32_t i = 0; i < numEventsInWaitList; i++) {
        auto event = castToObjectOrAbort<Event>(eventWaitList[i]);
        if (!event->isReadyForSubmission()) {
            userEventsDependencies = true;
            break;
        }
    }
    return userEventsDependencies;
}

uint32_t Event::peekTaskLevel() const {
    return taskLevel;
}

} // namespace NEO

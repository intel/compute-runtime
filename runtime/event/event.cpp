/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "public/cl_ext_private.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/event/event.h"
#include "runtime/event/event_tracker.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/api/cl_types.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/utilities/range.h"
#include "runtime/utilities/stackvec.h"
#include "runtime/utilities/tag_allocator.h"
#include "runtime/platform/platform.h"
#include "runtime/event/async_events_handler.h"

namespace OCLRT {

const cl_uint Event::eventNotReady = 0xFFFFFFF0;

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
    if (OCLRT::DebugManager.flags.EventsTrackerEnable.get()) {
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
        if (cmdQueue->getDevice().getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>(cmdQueue->getDevice().getMemoryManager());
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
    if (OCLRT::DebugManager.flags.EventsTrackerEnable.get()) {
        EventsTracker::getEventsTracker().notifyDestruction(this);
    }

    DBG_LOG(EventsDebugEnable, "~Event()", this);
    //no commands should be registred
    DEBUG_BREAK_IF(this->cmdToSubmit.load());

    submitCommand(true);

    int32_t lastStatus = executionStatus;
    if (isStatusCompleted(&lastStatus) == false) {
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
            TagAllocator<HwTimeStamps> *allocator = cmdQueue->getDevice().getMemoryManager()->peekEventTsAllocator();
            allocator->returnTag(timeStampNode);
        }
        if (perfCounterNode != nullptr) {
            TagAllocator<HwPerfCounter> *allocator = cmdQueue->getDevice().getMemoryManager()->peekEventPerfCountAllocator();
            allocator->returnTag(perfCounterNode);
        }
        cmdQueue->decRefInternal();
    }

    if (ctx != nullptr) {
        ctx->decRefInternal();
    }
    if (perfConfigurationData) {
        delete perfConfigurationData;
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
    size_t srcSize = 0;

    // CL_PROFILING_INFO_NOT_AVAILABLE if event refers to the clEnqueueSVMFree command
    if (isUserEvent() != CL_FALSE ||         // or is a user event object.
        !updateStatusAndCheckCompletion() || //if the execution status of the command identified by event is not CL_COMPLETE
        !profilingEnabled)                   // the CL_QUEUE_PROFILING_ENABLE flag is not set for the command-queue,
    {
        return CL_PROFILING_INFO_NOT_AVAILABLE;
    }

    // if paramValue is NULL, it is ignored
    switch (paramName) {
    case CL_PROFILING_COMMAND_QUEUED:
        src = &queueTimeStamp.CPUTimeinNS;
        if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
            src = &queueTimeStamp.GPUTimeStamp;
        }

        srcSize = sizeof(cl_ulong);
        break;

    case CL_PROFILING_COMMAND_SUBMIT:
        src = &submitTimeStamp.CPUTimeinNS;
        if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
            src = &submitTimeStamp.GPUTimeStamp;
        }
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
        if (!cmdQueue->getPerfCounters()->processEventReport(paramValueSize,
                                                             paramValue,
                                                             paramValueSizeRet,
                                                             getHwPerfCounterNode()->tag,
                                                             perfConfigurationData,
                                                             updateStatusAndCheckCompletion())) {
            return CL_PROFILING_INFO_NOT_AVAILABLE;
        }
        return CL_SUCCESS;
    default:
        return CL_INVALID_VALUE;
    }

    retVal = ::getInfo(paramValue, paramValueSize, src, srcSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcSize;
    }

    return retVal;
} // namespace OCLRT

uint32_t Event::getCompletionStamp() const {
    return this->taskCount;
}

void Event::updateCompletionStamp(uint32_t taskCount, uint32_t tasklevel, FlushStamp flushStamp) {
    this->taskCount = taskCount;
    this->taskLevel = tasklevel;
    this->flushStamp->setStamp(flushStamp);
}

cl_ulong Event::getDelta(cl_ulong startTime,
                         cl_ulong endTime) {
    cl_ulong Max = (1ULL << OCLRT_NUM_TIMESTAMP_BITS) - 1;
    cl_ulong Delta = 0;

    startTime &= Max;
    endTime &= Max;

    if (startTime > endTime) {
        Delta = Max - startTime;
        Delta += endTime;
    } else {
        Delta = endTime - startTime;
    }

    return Delta;
}

bool Event::calcProfilingData() {
    uint64_t gpuDuration = 0;
    uint64_t cpuDuration = 0;

    uint64_t gpuCompleteDuration = 0;
    uint64_t cpuCompleteDuration = 0;

    int64_t c0 = 0;
    if (!dataCalculated && timeStampNode && !profilingCpuPath) {
        double frequency = cmdQueue->getDevice().getDeviceInfo().profilingTimerResolution;
        /* calculation based on equation
           CpuTime = GpuTime * scalar + const( == c0)
           scalar = DeltaCpu( == dCpu) / DeltaGpu( == dGpu)
           to determine the value of the const we can use one pair of values
           const = CpuTimeQueue - GpuTimeQueue * scalar
        */

        //If device enqueue has not updated complete timestamp, assign end timestamp
        if (((HwTimeStamps *)timeStampNode->tag)->ContextCompleteTS == 0)
            ((HwTimeStamps *)timeStampNode->tag)->ContextCompleteTS = ((HwTimeStamps *)timeStampNode->tag)->ContextEndTS;

        c0 = queueTimeStamp.CPUTimeinNS - static_cast<uint64_t>(queueTimeStamp.GPUTimeStamp * frequency);
        gpuDuration = getDelta(
            ((HwTimeStamps *)timeStampNode->tag)->ContextStartTS,
            ((HwTimeStamps *)timeStampNode->tag)->ContextEndTS);
        gpuCompleteDuration = getDelta(
            ((HwTimeStamps *)timeStampNode->tag)->ContextStartTS,
            ((HwTimeStamps *)timeStampNode->tag)->ContextCompleteTS);
        cpuDuration = static_cast<uint64_t>(gpuDuration * frequency);
        cpuCompleteDuration = static_cast<uint64_t>(gpuCompleteDuration * frequency);
        startTimeStamp = static_cast<uint64_t>(((HwTimeStamps *)timeStampNode->tag)->GlobalStartTS * frequency) + c0;

        endTimeStamp = startTimeStamp + cpuDuration;
        completeTimeStamp = startTimeStamp + cpuCompleteDuration;

        if (DebugManager.flags.ReturnRawGpuTimestamps.get()) {
            startTimeStamp = ((HwTimeStamps *)timeStampNode->tag)->ContextStartTS;
            endTimeStamp = ((HwTimeStamps *)timeStampNode->tag)->ContextEndTS;
            completeTimeStamp = ((HwTimeStamps *)timeStampNode->tag)->ContextCompleteTS;
        }

        dataCalculated = true;
    }
    return dataCalculated;
}

inline bool Event::wait(bool blocking, bool useQuickKmdSleep) {
    while (this->taskCount == Event::eventNotReady) {
        if (blocking == false) {
            return false;
        }
    }

    cmdQueue->waitUntilComplete(taskCount.load(), flushStamp->peekStamp(), useQuickKmdSleep);
    updateExecutionStatus();

    DEBUG_BREAK_IF(this->taskLevel == Event::eventNotReady && this->executionStatus >= 0);

    auto *allocationStorage = cmdQueue->getDevice().getCommandStreamReceiver().getInternalAllocationStorage();
    allocationStorage->cleanAllocationList(this->taskCount, TEMPORARY_ALLOCATION);

    return true;
}

void Event::updateExecutionStatus() {
    if (taskLevel == Event::eventNotReady) {
        return;
    }

    int32_t statusSnapshot = executionStatus;
    if (isStatusCompleted(&statusSnapshot)) {
        executeCallbacks(statusSnapshot);
        return;
    }

    if (peekIsBlocked()) {
        transitionExecutionStatus(CL_QUEUED);
        executeCallbacks(CL_QUEUED);
        return;
    }

    if (statusSnapshot == CL_QUEUED) {
        bool abortBlockedTasks = isStatusCompletedByTermination(&statusSnapshot);
        submitCommand(abortBlockedTasks);
        transitionExecutionStatus(CL_SUBMITTED);
        executeCallbacks(CL_SUBMITTED);
        unblockEventsBlockedByThis(CL_SUBMITTED);
        // Note : Intentional fallthrough (no return) to check for CL_COMPLETE
    }

    if ((cmdQueue != nullptr) && (cmdQueue->isCompleted(getCompletionStamp()))) {
        transitionExecutionStatus(CL_COMPLETE);
        executeCallbacks(CL_COMPLETE);
        unblockEventsBlockedByThis(CL_COMPLETE);
        auto *allocationStorage = cmdQueue->getDevice().getCommandStreamReceiver().getInternalAllocationStorage();
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
    DEBUG_BREAK_IF(!(isStatusCompleted(&status) || (peekIsSubmitted(&status))));

    uint32_t taskLevelToPropagate = Event::eventNotReady;

    if (isStatusCompletedByTermination(&transitionStatus) == false) {
        //if we are event on top of the tree , obtain taskLevel from CSR
        if (taskLevel == Event::eventNotReady) {
            this->taskLevel = getTaskLevel();
            taskLevelToPropagate = this->taskLevel;
        } else {
            taskLevelToPropagate = taskLevel + 1;
        }
    }

    auto childEventRef = childEventsToNotify.detachNodes();
    while (childEventRef != nullptr) {
        auto childEvent = childEventRef->ref;

        childEvent->unblockEventBy(*this, taskLevelToPropagate, transitionStatus);

        if (childEvent->getCommandQueue() && childEvent->isCurrentCmdQVirtualEvent()) {
            // Check virtual event state and delete it if possible.
            childEvent->getCommandQueue()->isQueueBlocked();
        }

        childEvent->decRefInternal();
        auto next = childEventRef->next;
        delete childEventRef;
        childEventRef = next;
    }
}

bool Event::setStatus(cl_int status) {
    int32_t prevStatus = executionStatus;

    DBG_LOG(EventsDebugEnable, "setStatus event", this, " new status", status, "previousStatus", prevStatus);

    if (isStatusCompleted(&prevStatus)) {
        return false;
    }

    if (status == prevStatus) {
        return false;
    }

    if (peekIsBlocked() && (isStatusCompletedByTermination(&status) == false)) {
        return false;
    }

    if ((status == CL_SUBMITTED) || (isStatusCompleted(&status))) {
        bool abortBlockedTasks = isStatusCompletedByTermination(&status);
        submitCommand(abortBlockedTasks);
    }

    this->incRefInternal();
    transitionExecutionStatus(status);
    if (isStatusCompleted(&status) || (status == CL_SUBMITTED)) {
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
    if (OCLRT::DebugManager.flags.EventsTrackerEnable.get()) {
        EventsTracker::getEventsTracker().notifyTransitionedExecutionStatus();
    }
}

void Event::submitCommand(bool abortTasks) {
    std::unique_ptr<Command> cmdToProcess(cmdToSubmit.exchange(nullptr));
    if (cmdToProcess.get() != nullptr) {
        if ((this->isProfilingEnabled()) && (this->cmdQueue != nullptr)) {
            if (timeStampNode) {
                this->cmdQueue->getDevice().getCommandStreamReceiver().makeResident(*timeStampNode->getGraphicsAllocation());
                cmdToProcess->timestamp = timeStampNode->tag;
            }
            if (profilingCpuPath) {
                setSubmitTimeStamp();
                setStartTimeStamp();
            } else {
                this->cmdQueue->getDevice().getOSTime()->getCpuGpuTime(&submitTimeStamp);
            }
            if (perfCountersEnabled && perfCounterNode) {
                this->cmdQueue->getDevice().getCommandStreamReceiver().makeResident(*perfCounterNode->getGraphicsAllocation());
            }
        }
        auto &complStamp = cmdToProcess->submit(taskLevel, abortTasks);
        if (profilingCpuPath && this->isProfilingEnabled() && (this->cmdQueue != nullptr)) {
            setEndTimeStamp();
        }
        updateTaskCount(complStamp.taskCount);
        flushStamp->setStamp(complStamp.flushStamp);
        submittedCmd.exchange(cmdToProcess.release());
    } else if (profilingCpuPath && endTimeStamp == 0) {
        setEndTimeStamp();
    }
    if (this->taskCount == Event::eventNotReady) {
        if (!this->isUserEvent() && this->eventWithoutCommand) {
            if (this->cmdQueue) {
                TakeOwnershipWrapper<Device> deviceOwnerhsip(this->cmdQueue->getDevice());
                updateTaskCount(this->cmdQueue->getDevice().getCommandStreamReceiver().peekTaskCount());
            }
        }
    }
}

cl_int Event::waitForEvents(cl_uint numEvents,
                            const cl_event *eventList) {
    if (numEvents == 0) {
        return CL_SUCCESS;
    }

    //flush all command queues
    for (const cl_event *it = eventList, *end = eventList + numEvents; it != end; ++it) {
        Event *event = castToObjectOrAbort<Event>(*it);
        if (event->cmdQueue) {
            if (event->taskLevel != Event::eventNotReady) {
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

    while (currentlyPendingEvents->size() > 0) {
        for (auto &e : *currentlyPendingEvents) {
            Event *event = castToObjectOrAbort<Event>(e);
            if (event->peekExecutionStatus() < CL_COMPLETE) {
                return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            }

            if (event->wait(false, false) == false) {
                pendingEventsLeft->push_back(event);
            }
        }

        std::swap(currentlyPendingEvents, pendingEventsLeft);
        pendingEventsLeft->clear();
    }

    return CL_SUCCESS;
}

uint32_t Event::getTaskLevel() {
    return taskLevel;
}

inline void Event::unblockEventBy(Event &event, uint32_t taskLevel, int32_t transitionStatus) {
    int32_t numEventsBlockingThis = --parentCount;
    DEBUG_BREAK_IF(numEventsBlockingThis < 0);

    int32_t blockerStatus = transitionStatus;
    DEBUG_BREAK_IF(!(isStatusCompleted(&blockerStatus) || peekIsSubmitted(&blockerStatus)));

    if ((numEventsBlockingThis > 0) && (isStatusCompletedByTermination(&blockerStatus) == false)) {
        return;
    }
    DBG_LOG(EventsDebugEnable, "Event", this, "is unblocked by", &event);

    if (this->taskLevel == Event::eventNotReady) {
        this->taskLevel = taskLevel;
    } else {
        this->taskLevel = std::max(this->taskLevel.load(), taskLevel);
    }

    int32_t statusToPropagate = CL_SUBMITTED;
    if (isStatusCompletedByTermination(&blockerStatus)) {
        statusToPropagate = blockerStatus;
    }
    setStatus(statusToPropagate);

    //event may be completed after this operation, transtition the state to not block others.
    this->updateExecutionStatus();
}

bool Event::updateStatusAndCheckCompletion() {
    auto currentStatus = updateEventAndReturnCurrentStatus();
    return isStatusCompleted(&currentStatus);
}

bool Event::isReadyForSubmission() {
    return taskLevel != Event::eventNotReady ? true : false;
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
        platform()->getAsyncEventsHandler()->registerEvent(this);
    }

    decRefInternal();
}

void Event::executeCallbacks(int32_t executionStatusIn) {
    int32_t execStatus = executionStatusIn;
    bool terminated = isStatusCompletedByTermination(&execStatus);
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
    //only if event is not completed, completed event has already been flushed
    if (cmdQueue && updateStatusAndCheckCompletion() == false) {
        //flush the command queue only if it is not blocked event
        if (taskLevel != Event::eventNotReady) {
            cmdQueue->getDevice().getCommandStreamReceiver().flushBatchedSubmissions();
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

TagNode<HwTimeStamps> *Event::getHwTimeStampNode() {
    if (!timeStampNode) {
        auto &device = getCommandQueue()->getDevice();
        auto preferredPoolSize = device.getCommandStreamReceiver().getPreferredTagPoolSize();

        timeStampNode = device.getMemoryManager()->obtainEventTsAllocator(preferredPoolSize)->getTag();
    }
    return timeStampNode;
}

TagNode<HwPerfCounter> *Event::getHwPerfCounterNode() {
    if (!perfCounterNode) {
        auto &device = getCommandQueue()->getDevice();
        auto preferredPoolSize = device.getCommandStreamReceiver().getPreferredTagPoolSize();

        perfCounterNode = device.getMemoryManager()->obtainEventPerfCountAllocator(preferredPoolSize)->getTag();
    }
    return perfCounterNode;
}

void Event::copyPerfCounters(InstrPmRegsCfg *config) {
    perfConfigurationData = new InstrPmRegsCfg;
    memcpy_s(perfConfigurationData, sizeof(InstrPmRegsCfg), config, sizeof(InstrPmRegsCfg));
}

void Event::addTimestampPacketNodes(TimestampPacketContainer &inputTimestampPacketContainer) {
    timestampPacketContainer->assignAndIncrementNodesRefCounts(inputTimestampPacketContainer);
}

TimestampPacketContainer *Event::getTimestampPacketNodes() const { return timestampPacketContainer.get(); }
} // namespace OCLRT

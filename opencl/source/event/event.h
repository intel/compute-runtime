/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/idlist.h"
#include "shared/source/utilities/iflist.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/event/hw_timestamps.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/os_interface/performance_counters.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace NEO {
template <typename TagType>
struct TagNode;
class CommandQueue;
class Context;
class Device;
class TimestampPacketContainer;

template <>
struct OpenCLObjectMapper<_cl_event> {
    typedef class Event DerivedType;
};

class Event : public BaseObject<_cl_event>, public IDNode<Event> {
  public:
    enum class ECallbackTarget : uint32_t {
        Queued = 0,
        Submitted,
        Running,
        Completed,
        MAX,
        Invalid
    };

    struct Callback : public IFNode<Callback> {
        typedef void(CL_CALLBACK *ClbFuncT)(cl_event, cl_int, void *);

        Callback(cl_event event, ClbFuncT clb, cl_int type, void *data)
            : event(event), callbackFunction(clb), callbackExecutionStatusTarget(type), userData(data) {
        }

        void execute() {
            callbackFunction(event, callbackExecutionStatusTarget, userData);
        }

        int32_t getCallbackExecutionStatusTarget() const {
            return callbackExecutionStatusTarget;
        }

        // From OCL spec :
        //     "If the callback is called as the result of the command associated with
        //      event being abnormally terminated, an appropriate error code for the error that caused
        //      the termination will be passed to event_command_exec_status instead."
        // This function allows to override this value
        void overrideCallbackExecutionStatusTarget(int32_t newCallbackExecutionStatusTarget) {
            DEBUG_BREAK_IF(newCallbackExecutionStatusTarget >= 0);
            callbackExecutionStatusTarget = newCallbackExecutionStatusTarget;
        }

      private:
        cl_event event;
        ClbFuncT callbackFunction;
        int32_t callbackExecutionStatusTarget; // minimum event execution status that will triger this callback
        void *userData;
    };

    static const cl_ulong objectMagic = 0x80134213A43C981ALL;

    Event(CommandQueue *cmdQueue, cl_command_type cmdType,
          uint32_t taskLevel, uint32_t taskCount);

    Event(const Event &) = delete;
    Event &operator=(const Event &) = delete;

    ~Event() override;

    uint32_t getCompletionStamp(void) const;
    void updateCompletionStamp(uint32_t taskCount, uint32_t tasklevel, FlushStamp flushStamp);
    cl_ulong getDelta(cl_ulong startTime,
                      cl_ulong endTime);
    void setCPUProfilingPath(bool isCPUPath) { this->profilingCpuPath = isCPUPath; }
    bool isCPUProfilingPath() const {
        return profilingCpuPath;
    }

    cl_int getEventProfilingInfo(cl_profiling_info paramName,
                                 size_t paramValueSize,
                                 void *paramValue,
                                 size_t *paramValueSizeRet);

    bool isProfilingEnabled() const { return profilingEnabled; }

    void setProfilingEnabled(bool profilingEnabled) { this->profilingEnabled = profilingEnabled; }

    TagNode<HwTimeStamps> *getHwTimeStampNode();

    void addTimestampPacketNodes(const TimestampPacketContainer &inputTimestampPacketContainer);
    TimestampPacketContainer *getTimestampPacketNodes() const;

    bool isPerfCountersEnabled() const {
        return perfCountersEnabled;
    }

    void setPerfCountersEnabled(bool perfCountersEnabled) {
        this->perfCountersEnabled = perfCountersEnabled;
    }

    TagNode<HwPerfCounter> *getHwPerfCounterNode();

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::atomic<uint32_t> taskLevel;

    void addChild(Event &e);

    virtual bool setStatus(cl_int status);

    static cl_int waitForEvents(cl_uint numEvents,
                                const cl_event *eventList);

    void setCommand(std::unique_ptr<Command> newCmd) {
        UNRECOVERABLE_IF(cmdToSubmit.load());
        cmdToSubmit.exchange(newCmd.release());
        eventWithoutCommand = false;
    }
    Command *peekCommand() {
        return cmdToSubmit;
    }

    IFNodeRef<Event> *peekChildEvents() {
        return childEventsToNotify.peekHead();
    }

    bool peekHasChildEvents() {
        return (peekChildEvents() != nullptr);
    }

    bool peekHasCallbacks(ECallbackTarget target) {
        if (target >= ECallbackTarget::MAX) {
            DEBUG_BREAK_IF(true);
            return false;
        }
        return (callbacks[(uint32_t)target].peekHead() != nullptr);
    }

    bool peekHasCallbacks() {
        for (uint32_t i = 0; i < (uint32_t)ECallbackTarget::MAX; ++i) {
            if (peekHasCallbacks((ECallbackTarget)i)) {
                return true;
            }
        }
        return false;
    }

    // return the number of events that are blocking this event
    uint32_t peekNumEventsBlockingThis() const {
        return parentCount;
    }

    // returns true if event is completed (in terms of definition provided by OCL spec)
    // Note from OLC spec :
    //    "A command is considered complete if its execution status
    //     is CL_COMPLETE or a negative value."

    bool isStatusCompleted(const int32_t executionStatusSnapshot) {
        return executionStatusSnapshot <= CL_COMPLETE;
    }

    bool updateStatusAndCheckCompletion();

    // Note from OCL spec :
    //      "A negative integer value causes all enqueued commands that wait on this user event
    //       to be terminated."
    bool isStatusCompletedByTermination(const int32_t executionStatusSnapshot) const {
        return executionStatusSnapshot < 0;
    }

    bool peekIsSubmitted(const int32_t executionStatusSnapshot) const {
        return executionStatusSnapshot == CL_SUBMITTED;
    }

    bool peekIsCmdSubmitted() {
        return submittedCmd != nullptr;
    }

    //commands blocked by user event depencies
    bool isReadyForSubmission();

    // adds a callback (execution state change listener) to this event's list of callbacks
    void addCallback(Callback::ClbFuncT fn, cl_int type, void *data);

    //returns true on success
    //if(blocking==false), will return with false instead of blocking while waiting for completion
    virtual bool wait(bool blocking, bool useQuickKmdSleep);

    bool isUserEvent() const {
        return (CL_COMMAND_USER == cmdType);
    }

    bool isEventWithoutCommand() const {
        return eventWithoutCommand;
    }

    Context *getContext() {
        return ctx;
    }

    CommandQueue *getCommandQueue() {
        return cmdQueue;
    }

    cl_command_type getCommandType() {
        return cmdType;
    }

    virtual uint32_t getTaskLevel();

    cl_int peekExecutionStatus() const {
        return executionStatus;
    }

    cl_int updateEventAndReturnCurrentStatus() {
        updateExecutionStatus();
        return executionStatus;
    }

    bool peekIsBlocked() const {
        return (peekNumEventsBlockingThis() > 0);
    }

    virtual void unblockEventBy(Event &event, uint32_t taskLevel, int32_t transitionStatus);

    void updateTaskCount(uint32_t taskCount) {
        if (taskCount == CompletionStamp::levelNotReady) {
            DEBUG_BREAK_IF(true);
            return;
        }

        uint32_t prevTaskCount = this->taskCount.exchange(taskCount);
        if ((prevTaskCount != CompletionStamp::levelNotReady) && (prevTaskCount > taskCount)) {
            this->taskCount = prevTaskCount;
            DEBUG_BREAK_IF(true);
        }
    }

    bool isCurrentCmdQVirtualEvent() {
        return currentCmdQVirtualEvent;
    }

    void setCurrentCmdQVirtualEvent(bool isCurrentVirtualEvent) {
        currentCmdQVirtualEvent = isCurrentVirtualEvent;
    }

    virtual void updateExecutionStatus();
    void tryFlushEvent();

    uint32_t peekTaskCount() const {
        return this->taskCount;
    }

    void setQueueTimeStamp(TimeStampData *queueTimeStamp) {
        this->queueTimeStamp = *queueTimeStamp;
    };

    void setSubmitTimeStamp(TimeStampData *submitTimeStamp) {
        this->submitTimeStamp = *submitTimeStamp;
    };

    void setQueueTimeStamp();
    void setSubmitTimeStamp();

    void setStartTimeStamp();
    void setEndTimeStamp();

    void setCmdType(uint32_t cmdType) {
        this->cmdType = cmdType;
    }

    std::vector<Event *> &getParentEvents() { return this->parentEvents; }

    virtual bool isExternallySynchronized() const {
        return false;
    }

    static bool checkUserEventDependencies(cl_uint numEventsInWaitList, const cl_event *eventWaitList);

  protected:
    Event(Context *ctx, CommandQueue *cmdQueue, cl_command_type cmdType,
          uint32_t taskLevel, uint32_t taskCount);

    ECallbackTarget translateToCallbackTarget(cl_int execStatus) {
        switch (execStatus) {
        default: {
            DEBUG_BREAK_IF(true);
            return ECallbackTarget::Invalid;
        }

        case CL_QUEUED:
            return ECallbackTarget::Queued;
        case CL_SUBMITTED:
            return ECallbackTarget::Submitted;
        case CL_RUNNING:
            return ECallbackTarget::Running;
        case CL_COMPLETE:
            return ECallbackTarget::Completed;
        }
    }

    bool calcProfilingData();
    MOCKABLE_VIRTUAL void calculateProfilingDataInternal(uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS);
    MOCKABLE_VIRTUAL void synchronizeTaskCount() {
        while (this->taskCount == CompletionStamp::levelNotReady)
            ;
    };

    // executes all callbacks associated with this event
    void executeCallbacks(int32_t executionStatus);

    // transitions event to new execution state
    // guarantees that newStatus <= oldStatus
    void transitionExecutionStatus(int32_t newExecutionStatus) const;

    //vector storing events that needs to be notified when this event is ready to go
    IFRefList<Event, true, true> childEventsToNotify;
    void unblockEventsBlockedByThis(int32_t transitionStatus);
    void submitCommand(bool abortBlockedTasks);

    bool currentCmdQVirtualEvent;
    std::atomic<Command *> cmdToSubmit;
    std::atomic<Command *> submittedCmd;
    bool eventWithoutCommand = true;

    Context *ctx;
    CommandQueue *cmdQueue;
    cl_command_type cmdType;

    // callbacks to be executed when this event changes its execution state
    IFList<Callback, true, true> callbacks[(uint32_t)ECallbackTarget::MAX];

    // can be accessed only with transitionExecutionState
    // this is to ensure state consitency event when doning lock-free multithreading
    // e.g. CL_COMPLETE -> CL_SUBMITTED or CL_SUBMITTED -> CL_QUEUED becomes forbiden
    mutable std::atomic<int32_t> executionStatus;
    // Timestamps
    bool profilingEnabled;
    bool profilingCpuPath;
    bool dataCalculated;
    TimeStampData queueTimeStamp;
    TimeStampData submitTimeStamp;
    uint64_t startTimeStamp;
    uint64_t endTimeStamp;
    uint64_t completeTimeStamp;
    bool perfCountersEnabled;
    TagNode<HwTimeStamps> *timeStampNode = nullptr;
    TagNode<HwPerfCounter> *perfCounterNode = nullptr;
    std::unique_ptr<TimestampPacketContainer> timestampPacketContainer;
    //number of events this event depends on
    std::atomic<int> parentCount;
    //event parents
    std::vector<Event *> parentEvents;

  private:
    // can be accessed only with updateTaskCount
    std::atomic<uint32_t> taskCount;
};
} // namespace NEO

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/idlist.h"
#include "shared/source/utilities/iflist.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/command_queue/copy_engine_state.h"
#include "opencl/source/helpers/base_object.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace NEO {
class Command;
class TagNodeBase;
class FlushStampTracker;
template <typename TagType>
class TagNode;
class CommandQueue;
class Context;
class Device;
class TimestampPacketContainer;
enum class WaitStatus;

template <>
struct OpenCLObjectMapper<_cl_event> {
    typedef class Event DerivedType;
};

class Event : public BaseObject<_cl_event>, public IDNode<Event> {
  public:
    enum class ECallbackTarget : uint32_t {
        queued = 0,
        submitted,
        running,
        completed,
        max,
        invalid
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
        int32_t callbackExecutionStatusTarget; // minimum event execution status that will trigger this callback
        void *userData;
    };
    static_assert(NEO::NonCopyableAndNonMovable<IFList<Callback, true, true>>);

    struct ProfilingInfo {
        uint64_t cpuTimeInNs;
        uint64_t gpuTimeInNs;
        uint64_t gpuTimeStamp;
    };

    static const cl_ulong objectMagic = 0x80134213A43C981ALL;
    static constexpr cl_int executionAbortedDueToGpuHang = -777;

    Event(CommandQueue *cmdQueue, cl_command_type cmdType,
          TaskCountType taskLevel, TaskCountType taskCount);

    ~Event() override;

    void setupBcs(aub_stream::EngineType bcsEngineType);
    TaskCountType peekBcsTaskCountFromCommandQueue();
    bool isBcsEvent() const;
    aub_stream::EngineType getBcsEngineType() const;

    TaskCountType getCompletionStamp() const;
    void updateCompletionStamp(TaskCountType taskCount, TaskCountType bcsTaskCount, TaskCountType tasklevel, FlushStamp flushStamp);
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

    TagNodeBase *getHwTimeStampNode();

    void addTimestampPacketNodes(const TimestampPacketContainer &inputTimestampPacketContainer);
    TimestampPacketContainer *getTimestampPacketNodes() const;
    TimestampPacketContainer *getMultiRootDeviceTimestampPacketNodes() const;

    bool isPerfCountersEnabled() const {
        return perfCountersEnabled;
    }

    void setPerfCountersEnabled(bool perfCountersEnabled) {
        this->perfCountersEnabled = perfCountersEnabled;
    }

    void abortExecutionDueToGpuHang() {
        this->transitionExecutionStatus(executionAbortedDueToGpuHang);
    }

    TagNodeBase *getHwPerfCounterNode();
    TagNodeBase *getMultiRootTimestampSyncNode();

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::atomic<TaskCountType> taskLevel;

    TaskCountType peekTaskLevel() const;
    void addChild(Event &e);

    virtual bool setStatus(cl_int status);

    static cl_int waitForEvents(cl_uint numEvents,
                                const cl_event *eventList);

    void setCommand(std::unique_ptr<Command> newCmd);

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
        if (target >= ECallbackTarget::max) {
            DEBUG_BREAK_IF(true);
            return false;
        }
        return (callbacks[static_cast<uint32_t>(target)].peekHead() != nullptr);
    }

    bool peekHasCallbacks() {
        for (uint32_t i = 0; i < static_cast<uint32_t>(ECallbackTarget::max); ++i) {
            if (peekHasCallbacks(static_cast<ECallbackTarget>(i))) {
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
    bool isCompleted();

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

    // commands blocked by user event dependencies
    bool isReadyForSubmission();

    // adds a callback (execution state change listener) to this event's list of callbacks
    void addCallback(Callback::ClbFuncT fn, cl_int type, void *data);

    // if(blocking==false), will return with WaitStatus::notReady instead of blocking while waiting for completion
    virtual WaitStatus wait(bool blocking, bool useQuickKmdSleep);

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

    const CommandQueue *getCommandQueue() const {
        return cmdQueue;
    }

    cl_command_type getCommandType() const {
        return cmdType;
    }

    virtual TaskCountType getTaskLevel();

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

    virtual void unblockEventBy(Event &event, TaskCountType taskLevel, int32_t transitionStatus);

    void updateTaskCount(TaskCountType gpgpuTaskCount, TaskCountType bcsTaskCount) {
        if (gpgpuTaskCount == CompletionStamp::notReady) {
            DEBUG_BREAK_IF(true);
            return;
        }

        this->bcsState.taskCount = bcsTaskCount;
        TaskCountType prevTaskCount = this->taskCount.exchange(gpgpuTaskCount);
        if ((prevTaskCount != CompletionStamp::notReady) && (prevTaskCount > gpgpuTaskCount)) {
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
    bool tryFlushEvent();

    TaskCountType peekTaskCount() const {
        return this->taskCount;
    }

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

    static void getBoundaryTimestampValues(TimestampPacketContainer *timestampContainer, uint64_t &globalStartTS, uint64_t &globalEndTS);

    void copyTimestamps(Event &srcEvent);

    void setWaitForTaskCountRequired(bool newValue) {
        waitForTaskCountRequired = newValue;
    }

    bool getWaitForTaskCountRequired() const {
        return waitForTaskCountRequired;
    }

  protected:
    Event(Context *ctx, CommandQueue *cmdQueue, cl_command_type cmdType,
          TaskCountType taskLevel, TaskCountType taskCount);

    ECallbackTarget translateToCallbackTarget(cl_int execStatus) {
        switch (execStatus) {
        default: {
            DEBUG_BREAK_IF(true);
            return ECallbackTarget::invalid;
        }

        case CL_QUEUED:
            return ECallbackTarget::queued;
        case CL_SUBMITTED:
            return ECallbackTarget::submitted;
        case CL_RUNNING:
            return ECallbackTarget::running;
        case CL_COMPLETE:
            return ECallbackTarget::completed;
        }
    }

    uint64_t getProfilingInfoData(const ProfilingInfo &profilingInfo) const;
    void setupRelativeProfilingInfo(ProfilingInfo &profilingInfo);
    bool calcProfilingData();
    MOCKABLE_VIRTUAL void calculateProfilingDataInternal(uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS);
    MOCKABLE_VIRTUAL void synchronizeTaskCount() {
        while (this->taskCount == CompletionStamp::notReady)
            ;
    };

    // executes all callbacks associated with this event
    void executeCallbacks(int32_t executionStatus);

    // transitions event to new execution state
    // guarantees that newStatus <= oldStatus
    void transitionExecutionStatus(int32_t newExecutionStatus) const;

    // vector storing events that needs to be notified when this event is ready to go
    IFRefList<Event, true, true> childEventsToNotify;
    void unblockEventsBlockedByThis(int32_t transitionStatus);
    void submitCommand(bool abortBlockedTasks);

    static void setExecutionStatusToAbortedDueToGpuHang(cl_event *first, cl_event *last);

    bool isWaitForTimestampsEnabled() const;
    bool areTimestampsCompleted();

    void updateTimestamp(ProfilingInfo &timestamp, uint64_t newGpuTimestamp) const;
    void addOverflowToTimestamp(uint64_t &timestamp, uint64_t timestampWithOverflow) const;

    bool currentCmdQVirtualEvent = false;
    std::atomic<Command *> cmdToSubmit{nullptr};
    std::atomic<Command *> submittedCmd{nullptr};
    bool eventWithoutCommand = true;
    bool waitForTaskCountRequired = false;

    Context *ctx = nullptr;
    CommandQueue *cmdQueue = nullptr;
    cl_command_type cmdType{};

    // callbacks to be executed when this event changes its execution state
    IFList<Callback, true, true> callbacks[static_cast<uint32_t>(ECallbackTarget::max)];

    // can be accessed only with transitionExecutionState
    // this is to ensure state consistency event when doning lock-free multithreading
    // e.g. CL_COMPLETE -> CL_SUBMITTED or CL_SUBMITTED -> CL_QUEUED becomes forbidden
    mutable std::atomic<int32_t> executionStatus{CL_QUEUED};
    // Timestamps
    bool profilingEnabled = false;
    bool profilingCpuPath = false;
    bool dataCalculated = false;

    ProfilingInfo queueTimeStamp{};
    ProfilingInfo submitTimeStamp{};
    ProfilingInfo startTimeStamp{};
    ProfilingInfo endTimeStamp{};
    ProfilingInfo completeTimeStamp{};

    CopyEngineState bcsState{};
    bool perfCountersEnabled = false;
    TagNodeBase *timeStampNode = nullptr;
    TagNodeBase *perfCounterNode = nullptr;
    TagNodeBase *multiRootTimeStampSyncNode = nullptr;
    std::unique_ptr<TimestampPacketContainer> timestampPacketContainer;
    // number of events this event depends on
    std::unique_ptr<TimestampPacketContainer> multiRootDeviceTimestampPacketContainer;
    std::atomic<int> parentCount{0u};
    std::atomic<bool> gpuStateWaited{false};
    // event parents
    std::vector<Event *> parentEvents;

  private:
    // can be accessed only with updateTaskCount
    std::atomic<TaskCountType> taskCount{0};
};

static_assert(NEO::NonCopyableAndNonMovable<Event>);
} // namespace NEO

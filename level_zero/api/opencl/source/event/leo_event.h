/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/profiling_info.h"
#include "shared/source/utilities/iflist.h"

#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/sharings/gl/leo_gl_arb_sync_event.h"
#include "level_zero/core/source/event/event.h"

#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <span>
#include <variant>
#include <vector>

namespace NEO {
class TagNodeBase;
}

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_event> {
    typedef class Event DerivedType;
};

class EventHandleSpan {
  public:
    static constexpr uint32_t maxInlineWaitEvents = 16u;

    EventHandleSpan() noexcept = default;
    ~EventHandleSpan() = default;
    EventHandleSpan(const EventHandleSpan &) = delete;
    EventHandleSpan &operator=(const EventHandleSpan &) = delete;
    EventHandleSpan(EventHandleSpan &&) noexcept = default;
    EventHandleSpan &operator=(EventHandleSpan &&) noexcept = default;

    explicit EventHandleSpan(cl_uint numEvents, const cl_event *eventWaitList) noexcept;

    [[nodiscard]] uint32_t size() const noexcept { return count; }
    [[nodiscard]] ze_event_handle_t *data() noexcept {
        if (count == 0) {
            return nullptr;
        }
        return std::holds_alternative<InlineStorage>(storage) ? std::get<InlineStorage>(storage).data() : std::get<HeapStorage>(storage).data();
    }

  private:
    using InlineStorage = std::array<ze_event_handle_t, maxInlineWaitEvents>;
    using HeapStorage = std::vector<ze_event_handle_t>;
    std::variant<InlineStorage, HeapStorage> storage{InlineStorage{}};
    uint32_t count = 0;
};

static_assert(!std::is_copy_constructible_v<EventHandleSpan>);
static_assert(!std::is_copy_assignable_v<EventHandleSpan>);
static_assert(std::is_nothrow_move_constructible_v<EventHandleSpan>);
static_assert(std::is_nothrow_move_assignable_v<EventHandleSpan>);

class Event : public BaseObject<_cl_event> {
  public:
    static const cl_ulong objectMagic = 0x80134213A43C981ALL;
    static constexpr cl_int executionAbortedDueToGpuHang = -777;
    static constexpr cl_int executionTerminatedOnDestruction = -1;
    static constexpr uint64_t asyncCompletionWaitTimeoutNs = 100'000'000ull;

    struct Callback {
        typedef void(CL_CALLBACK *ClbFuncT)(cl_event, cl_int, void *);

        Callback(cl_event event, ClbFuncT clb, cl_int type, void *data)
            : event(event), callbackFunction(clb), callbackExecutionStatusTarget(type), userData(data) {
        }

        void execute() {
            callbackFunction(event, callbackExecutionStatusTarget, userData);
        }

        void overrideCallbackExecutionStatusTarget(cl_int newCallbackExecutionStatusTarget) {
            DEBUG_BREAK_IF(newCallbackExecutionStatusTarget >= 0);
            callbackExecutionStatusTarget = newCallbackExecutionStatusTarget;
        }

      private:
        cl_event event;
        ClbFuncT callbackFunction;
        cl_int callbackExecutionStatusTarget;
        void *userData;
    };

    using ProfilingInfo = NEO::ProfilingInfo;

    explicit Event(cl_command_type commandType, NEO::LEO::CommandQueue *commandQueue);
    explicit Event(NEO::LEO::Context *context);
    Event() = delete;
    ~Event() override;

    static std::pair<EventHandleSpan, ze_event_handle_t> setupEvents(cl_uint numEventsInWaitList, const cl_event *eventWaitList,
                                                                     cl_event *event, cl_command_type commandType,
                                                                     NEO::LEO::CommandQueue *commandQueue);

    cl_int getProfilingInfo(cl_profiling_info paramName,
                            size_t paramValueSize,
                            void *paramValue,
                            size_t *paramValueSizeRet);

    cl_int getEventInfo(cl_event_info paramName,
                        size_t paramValueSize,
                        void *paramValue,
                        size_t *paramValueSizeRet);

    ze_result_t wait() {
        return this->wait(std::numeric_limits<uint64_t>::max());
    }
    MOCKABLE_VIRTUAL ze_result_t wait(uint64_t timeout);
    ze_result_t signal(cl_int executionStatus);
    cl_int queryAndUpdateEventStatus();
    MOCKABLE_VIRTUAL ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t &result);

    cl_int peekExecutionStatus() const { return this->eventStatus.load(); };

    void addCallback(Callback::ClbFuncT fn, cl_int type, void *data);
    bool peekHasCallbacks();
    void updateExecutionStatus();
    void abortExecutionDueToGpuHang();

    void updateCommandType(cl_command_type newType) { this->commandType = newType; };
    cl_command_type getCommandType() const { return this->commandType; };
    bool isUserEvent() const { return std::holds_alternative<Context *>(oclObj); };
    Context *getContext() const { return this->isUserEvent() ? std::get<Context *>(oclObj) : std::get<CommandQueue *>(oclObj)->getContext(); };
    CommandQueue *getCommandQueue() const { return std::get<CommandQueue *>(oclObj); };

    GlArbSyncEvent *getGlArbSyncEvent() const { return this->arbEvent.get(); };
    void setGlArbSyncEvent(GlArbSyncEvent *arbEvent) { this->arbEvent.reset(arbEvent); };

    ze_event_handle_t getL0Handle() const { return this->eventHandle; };
    L0::Event *getL0Object() const { return L0::Event::fromHandle(this->eventHandle); };

    bool isPerfCountersEnabled() const { return perfCountersEnabled; };
    TagNodeBase *getHwPerfCounterNode();

  protected:
    enum class EventBacking { counterBased,
                              regular,
                              regularTimestamp };

    void setQueueTimeStamp();
    void setSubmitTimeStamp();
    void setupRelativeProfilingInfo(ProfilingInfo &profilingInfo);

    void executeCallbacks(cl_int executionStatus);

    std::mutex callbacksMtx;
    std::vector<Callback> callbacks;

    ProfilingInfo queueTimeStamp{};
    ProfilingInfo submitTimeStamp{};
    ProfilingInfo startTimeStamp{};
    ProfilingInfo endTimeStamp{};
    ProfilingInfo completeTimeStamp{};
    bool dataCalculated = false;

    cl_command_type commandType = 0;
    std::atomic<cl_int> eventStatus = CL_SUBMITTED;

    std::unique_ptr<GlArbSyncEvent> arbEvent = nullptr;

    ze_event_handle_t eventHandle = nullptr;
    EventBacking backing = EventBacking::counterBased;
    std::variant<CommandQueue *, Context *> oclObj{};
    TagNodeBase *perfCounterNode = nullptr;
    bool perfCountersEnabled = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Event>);

} // namespace LEO
} // namespace NEO

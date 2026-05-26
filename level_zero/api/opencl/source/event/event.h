/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "level_zero/core/source/event/event.h"

#include <array>
#include <span>
#include <variant>

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

    struct ProfilingInfo {
        uint64_t cpuTimeInNs;
        uint64_t gpuTimeInNs;
        uint64_t gpuTimeStamp;
    };

    struct ClUserData {
        cl_event event = nullptr;
        cl_int commandExecCallbackType{};
        void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *) = nullptr;
        void *userData = nullptr;
    };

    static void clCallbackWrapper(void *userData) {
        auto clUserData = static_cast<ClUserData *>(userData);
        clUserData->funcNotify(clUserData->event, clUserData->commandExecCallbackType, clUserData->userData);
        NEO::LEO::castToObject<NEO::LEO::Event>(clUserData->event)->decRefInternal();
        delete clUserData;
    }

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

    ze_result_t wait();
    ze_result_t signal();
    cl_int queryAndUpdateEventStatus();

    void updateCommandType(cl_command_type newType) { this->commandType = newType; };
    cl_command_type getCommandType() const { return this->commandType; };
    bool isUserEvent() const { return std::holds_alternative<Context *>(oclObj); };
    Context *getContext() const { return this->isUserEvent() ? std::get<Context *>(oclObj) : std::get<CommandQueue *>(oclObj)->getContext(); };
    CommandQueue *getCommandQueue() const { return std::get<CommandQueue *>(oclObj); };

    GlArbSyncEvent *getGlArbSyncEvent() const { return this->arbEvent.get(); };
    void setGlArbSyncEvent(GlArbSyncEvent *arbEvent) { this->arbEvent.reset(arbEvent); };

    ze_event_handle_t getL0Handle() const { return this->eventHandle; };
    L0::Event *getL0Object() const { return L0::Event::fromHandle(this->eventHandle); };

  protected:
    void setQueueTimeStamp();
    void setSubmitTimeStamp();
    void setupRelativeProfilingInfo(ProfilingInfo &profilingInfo);

    ProfilingInfo queueTimeStamp{};
    ProfilingInfo submitTimeStamp{};

    cl_command_type commandType = 0;
    cl_int eventStatus = CL_SUBMITTED;

    std::unique_ptr<GlArbSyncEvent> arbEvent = nullptr;

    ze_event_handle_t eventHandle = nullptr;
    std::variant<CommandQueue *, Context *> oclObj{};
};

static_assert(NEO::NonCopyableAndNonMovable<Event>);

} // namespace LEO
} // namespace NEO

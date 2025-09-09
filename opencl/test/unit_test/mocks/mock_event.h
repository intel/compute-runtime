/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/wait_status.h"

#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"

#include <optional>

namespace NEO {

#define FORWARD_CONSTRUCTOR(THIS_CLASS, BASE_CLASS)                          \
    template <typename... ArgsT>                                             \
    THIS_CLASS(ArgsT &&...args) : BASE_CLASS(std::forward<ArgsT>(args)...) { \
    }

#define FORWARD_FUNC(FUNC_NAME, BASE_CLASS)                  \
    template <typename... ArgsT>                             \
    void FUNC_NAME(ArgsT &&...args) {                        \
        BASE_CLASS::FUNC_NAME(std::forward<ArgsT>(args)...); \
    }

template <typename BaseEventType>
struct MockEvent : public BaseEventType {
    FORWARD_CONSTRUCTOR(MockEvent, BaseEventType);

    // make some protected members public :
    FORWARD_FUNC(submitCommand, BaseEventType);

    using BaseEventType::timeStampNode;
    using Event::areTimestampsCompleted;
    using Event::calcProfilingData;
    using Event::cmdToSubmit;
    using Event::isWaitForTimestampsEnabled;
    using Event::magic;
    using Event::multiRootDeviceTimestampPacketContainer;
    using Event::queueTimeStamp;
    using Event::startTimeStamp;
    using Event::submitTimeStamp;
    using Event::timestampPacketContainer;

    WaitStatus wait(bool blocking, bool useQuickKmdSleep) override {
        ++waitCalled;
        if (waitReturnValue.has_value()) {
            return *waitReturnValue;
        }

        return BaseEventType::wait(blocking, useQuickKmdSleep);
    }

    std::optional<WaitStatus> waitReturnValue{};
    int waitCalled = 0;
};

#undef FORWARD_CONSTRUCTOR
#undef FORWARD_FUNC

struct MockEventBuilder : EventBuilder {
    MockEventBuilder() = default;
    MockEventBuilder(Event *ev) {
        setEvent(ev);
    }

    void setEvent(Event *ev) {
        this->event = ev;
    }

    template <typename EventType, typename... ArgsT>
    static EventType *createAndFinalize(ArgsT &&...args) {
        MockEventBuilder mb;
        mb.create<EventType>(std::forward<ArgsT>(args)...);
        return static_cast<EventType *>(mb.finalizeAndRelease());
    }
};
} // namespace NEO

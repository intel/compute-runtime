/*
 * Copyright (C) 2018-2026 Intel Corporation
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

template <typename BaseEventType>
struct MockEvent : public BaseEventType {

    template <typename... ArgsT>
    MockEvent(ArgsT &&...args) : BaseEventType(std::forward<ArgsT>(args)...) {}

    using BaseEventType::submitCommand;
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

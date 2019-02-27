/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"

namespace OCLRT {

#define FORWARD_CONSTRUCTOR(THIS_CLASS, BASE_CLASS)                           \
    template <typename... ArgsT>                                              \
    THIS_CLASS(ArgsT &&... args) : BASE_CLASS(std::forward<ArgsT>(args)...) { \
    }

#define FORWARD_FUNC(FUNC_NAME, BASE_CLASS)                  \
    template <typename... ArgsT>                             \
    void FUNC_NAME(ArgsT &&... args) {                       \
        BASE_CLASS::FUNC_NAME(std::forward<ArgsT>(args)...); \
    }

template <typename BaseEventType>
struct MockEvent : public BaseEventType {
    FORWARD_CONSTRUCTOR(MockEvent, BaseEventType);

    // make some protected members public :
    FORWARD_FUNC(submitCommand, BaseEventType);

    using BaseEventType::timeStampNode;
    using Event::calcProfilingData;
    using Event::magic;
    using Event::queueTimeStamp;
    using Event::submitTimeStamp;
    using Event::timestampPacketContainer;
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
    static EventType *createAndFinalize(ArgsT &&... args) {
        MockEventBuilder mb;
        mb.create<EventType>(std::forward<ArgsT>(args)...);
        return static_cast<EventType *>(mb.finalizeAndRelease());
    }
};
} // namespace OCLRT

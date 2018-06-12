/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/event/event.h"
#include "runtime/event/event_builder.h"

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
    using Event::magic;
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

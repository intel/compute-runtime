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
#include "CL/cl.h"
#include <type_traits>
#include "runtime/utilities/arrayref.h"
#include "runtime/utilities/stackvec.h"

namespace OCLRT {

class Event;

class EventBuilder {
  public:
    template <typename EventType, typename... ArgsT>
    void create(ArgsT &&... args) {
        event = new EventType(std::forward<ArgsT>(args)...);
    }

    EventBuilder() = default;
    EventBuilder(const EventBuilder &) = delete;
    EventBuilder &operator=(const EventBuilder &) = delete;
    EventBuilder(EventBuilder &&) = delete;
    EventBuilder &operator=(EventBuilder &&) = delete;

    ~EventBuilder();

    Event *getEvent() {
        return event;
    }

    void addParentEvent(Event &newParentEvent);
    void addParentEvent(Event *newParentEvent) {
        if (newParentEvent != nullptr) {
            addParentEvent(*newParentEvent);
        }
    }

    void addParentEvents(ArrayRef<const cl_event> newParentEvents);

    void finalize();

    Event *finalizeAndRelease() {
        finalize();
        Event *retEvent = this->event;
        this->event = nullptr;
        finalized = false;
        return retEvent;
    }

  protected:
    void clear();

    Event *event = nullptr;
    bool finalized = false;
    StackVec<Event *, 16> parentEvents;
    bool doNotRegister = false;
};
} // namespace OCLRT

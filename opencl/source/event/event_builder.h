/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <type_traits>

namespace NEO {

class Event;

class EventBuilder : NEO::NonCopyableAndNonMovableClass {
  public:
    template <typename EventType, typename... ArgsT>
    void create(ArgsT &&...args) {
        event = new EventType(std::forward<ArgsT>(args)...);
    }

    EventBuilder() = default;

    ~EventBuilder();

    Event *getEvent() const {
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

static_assert(NEO::NonCopyableAndNonMovable<EventBuilder>);

} // namespace NEO

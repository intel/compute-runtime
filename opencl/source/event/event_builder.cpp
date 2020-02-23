/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/event/event_builder.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/user_event.h"

namespace NEO {
EventBuilder::~EventBuilder() {
    UNRECOVERABLE_IF((this->event == nullptr) && ((parentEvents.size() != 0U)));
    finalize();
}

void EventBuilder::addParentEvent(Event &newParentEvent) {
    bool duplicate = false;
    for (Event *parent : parentEvents) {
        if (parent == &newParentEvent) {
            duplicate = true;
            break;
        }
    }

    if (!duplicate) {
        newParentEvent.incRefInternal();
        parentEvents.push_back(&newParentEvent);
    }
}

void EventBuilder::addParentEvents(ArrayRef<const cl_event> newParentEvents) {
    for (cl_event clEv : newParentEvents) {
        auto neoEv = castToObject<Event>(clEv);
        DEBUG_BREAK_IF(neoEv == nullptr);
        addParentEvent(neoEv);
    }
}

void EventBuilder::finalize() {
    if ((this->event == nullptr) || finalized) {
        clear();
        return;
    }
    if (parentEvents.size() != 0) {
        UserEvent sentinel;
        sentinel.addChild(*this->event);
        for (Event *parent : parentEvents) {

            //do not add as child if:
            //parent has no parents and is not blocked
            if (!(parent->peekIsBlocked() == false && parent->taskLevel != CompletionStamp::levelNotReady) ||
                (!parent->isEventWithoutCommand() && !parent->peekIsCmdSubmitted())) {
                parent->addChild(*this->event);
            }
        }
        sentinel.setStatus(CL_COMPLETE);
    }

    clear();

    finalized = true;
}

void EventBuilder::clear() {
    for (Event *parent : parentEvents) {
        parent->decRefInternal();
    }

    parentEvents.clear();
}
} // namespace NEO

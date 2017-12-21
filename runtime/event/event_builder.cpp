/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/api/cl_types.h"
#include "runtime/context/context.h"
#include "runtime/event/event_builder.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/debug_helpers.h"

namespace OCLRT {
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
            if (!(parent->peekIsBlocked() == false && parent->taskLevel != Event::eventNotReady)) {
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
} // namespace OCLRT

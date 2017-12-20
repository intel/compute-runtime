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

#include "runtime/device/device.h"
#include "runtime/event/event_registry.h"

namespace OCLRT {
void EventsRegistry::broadcastUpdateAll() {
    this->processLocked<EventsRegistry, &EventsRegistry::updateSingleImpl>(nullptr, nullptr);
}

void EventsRegistry::setDevice(Device *device) {
    DEBUG_BREAK_IF(this->device != nullptr);
    device->incRefInternal();
    this->device = device;
}

EventsRegistry::~EventsRegistry() {
    if (device) {
        device->decRefInternal();
    }
}

Event *EventsRegistry::updateSingleImpl(Event *, void *) {
    executeDeferredDeletionList();
    Event *curr = head;
    while (curr != nullptr) {
        curr->updateExecutionStatus();
        curr = curr->next;
    }
    executeDeferredDeletionList();
    return nullptr;
}

void EventsRegistry::unregisterEvent(std::unique_ptr<Event> event) {
    if (this->lockOwner == std::this_thread::get_id()) {
        // current thread is processing the list - add event to deferred deletion list
        deferredDeletion.pushRefFrontOne(*event.release());
    } else {
        this->removeOne(*event.release());
    }
}
} // namespace OCLRT

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "events_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {

bool EventsImp::eventListen(zes_event_type_flags_t &pEvent) {
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
        return pOsEvents->isResetRequired(pEvent);
    }
    return false;
}

void EventsImp::init() {
    pOsEvents = OsEvents::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsEvents);
}

EventsImp::~EventsImp() {
    if (nullptr != pOsEvents) {
        delete pOsEvents;
    }
}

} // namespace L0

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/events/sysman_events_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {
namespace Sysman {

ze_result_t EventsImp::eventRegister(zes_event_type_flags_t events) {
    initEvents();
    return pOsEvents->eventRegister(events);
}

bool EventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    initEvents();
    return pOsEvents->eventListen(pEvent, timeout);
}
void EventsImp::initEvents() {
    std::call_once(initEventsOnce, [this]() {
        this->init();
    });
}
void EventsImp::init() {
    if (pOsEvents == nullptr) {
        pOsEvents = OsEvents::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsEvents);
}

EventsImp::~EventsImp() {
    if (nullptr != pOsEvents) {
        delete pOsEvents;
    }
}

} // namespace Sysman
} // namespace L0

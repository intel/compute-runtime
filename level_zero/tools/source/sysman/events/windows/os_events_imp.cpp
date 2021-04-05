/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/windows/os_events_imp.h"

#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {

void WddmEventsImp::registerEvents(zes_event_type_flags_t eventId, uint32_t requestId) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    EventHandler event;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.requestId = requestId;
    request.commandId = KmdSysman::Command::RegisterEvent;
    request.componentId = KmdSysman::Component::InterfaceProperties;
    request.dataSize = sizeof(HANDLE);

    event.requestId = requestId;
    event.id = eventId;

    event.windowsHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    memcpy_s(request.dataBuffer, sizeof(HANDLE), &event.windowsHandle, sizeof(HANDLE));

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        CloseHandle(event.windowsHandle);
        return;
    }

    eventList.push_back(event);
}

void WddmEventsImp::unregisterEvents() {
    ze_result_t status = ZE_RESULT_SUCCESS;
    EventHandler event;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    SetEvent(exitHandle);

    request.commandId = KmdSysman::Command::UnregisterEvent;
    request.componentId = KmdSysman::Component::InterfaceProperties;
    request.dataSize = sizeof(HANDLE);

    for (uint32_t i = 0; i < eventList.size(); i++) {
        request.requestId = eventList[i].requestId;
        event.windowsHandle = eventList[i].windowsHandle;

        memcpy_s(request.dataBuffer, sizeof(HANDLE), &event.windowsHandle, sizeof(HANDLE));

        status = pKmdSysManager->requestSingle(request, response);

        if (status == ZE_RESULT_SUCCESS) {
            CloseHandle(event.windowsHandle);
        }
    }

    eventList.clear();
}

ze_result_t WddmEventsImp::eventRegister(zes_event_type_flags_t events) {

    unregisterEvents();

    if (events & ZES_EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED) {
        registerEvents(ZES_EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED, KmdSysman::Events::EnergyThresholdCrossed);
    }

    if (events & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER) {
        registerEvents(ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER, KmdSysman::Events::EnterD3);
    }

    if (events & ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT) {
        registerEvents(ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT, KmdSysman::Events::EnterD0);
    }

    if (events & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
        registerEvents(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED, KmdSysman::Events::EnterTDR);
    }

    return (eventList.size() == 0) ? ZE_RESULT_ERROR_UNSUPPORTED_FEATURE : ZE_RESULT_SUCCESS;
}

bool WddmEventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    HANDLE events[MAXIMUM_WAIT_OBJECTS];
    pEvent = 0;

    if (eventList.size() == 0) {
        return false;
    }

    for (uint32_t i = 0; i < eventList.size(); i++) {
        events[i] = eventList[i].windowsHandle;
    }

    events[eventList.size()] = exitHandle;

    // Setting the last handle for the exit handle, then the exit handle is signaled, it breaks from the wait.
    uint32_t signaledEvent = WaitForMultipleObjects(static_cast<uint32_t>(eventList.size() + 1), events, FALSE, static_cast<uint32_t>(timeout));

    // Was a timeout
    if (signaledEvent == WAIT_TIMEOUT) {
        return false;
    }

    // Was the exit event
    if (signaledEvent >= eventList.size()) {
        ResetEvent(exitHandle);
        return false;
    }

    pEvent = eventList[signaledEvent].id;

    return true;
}

WddmEventsImp::WddmEventsImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    exitHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    WddmEventsImp *pWddmEventsImp = new WddmEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pWddmEventsImp);
}

} // namespace L0

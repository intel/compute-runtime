/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_driver_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

#include <sys/stat.h>

namespace L0 {

ze_result_t LinuxSysmanDriverImp::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    return pLinuxEventsUtil->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

void LinuxSysmanDriverImp::eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice) {
    pLinuxEventsUtil->eventRegister(events, pSysmanDevice);
}

L0::UdevLib *LinuxSysmanDriverImp::getUdevLibHandle() {
    if (pUdevLib == nullptr) {
        pUdevLib = UdevLib::create();
    }
    return pUdevLib;
}

LinuxSysmanDriverImp::LinuxSysmanDriverImp() {
    pLinuxEventsUtil = new LinuxEventsUtil();
}

LinuxSysmanDriverImp::~LinuxSysmanDriverImp() {
    if (nullptr != pUdevLib) {
        delete pUdevLib;
        pUdevLib = nullptr;
    }

    if (nullptr != pLinuxEventsUtil) {
        delete pLinuxEventsUtil;
        pLinuxEventsUtil = nullptr;
    }
}

OsSysmanDriver *OsSysmanDriver::create() {
    LinuxSysmanDriverImp *pLinuxSysmanDriverImp = new LinuxSysmanDriverImp();
    DEBUG_BREAK_IF(nullptr == pLinuxSysmanDriverImp);
    return static_cast<OsSysmanDriver *>(pLinuxSysmanDriverImp);
}

void __attribute__((destructor)) osSysmanDriverDestructor() {
    if (globalOsSysmanDriver != nullptr) {
        delete globalOsSysmanDriver;
        globalOsSysmanDriver = nullptr;
    }
}

} // namespace L0

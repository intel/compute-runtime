/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

#include "sysman/events/events_imp.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

bool LinuxEventsImp::isResetRequired(zes_event_type_flags_t &pEvent) {
    zes_device_state_t pState = {};
    if (pLinuxSysmanImp->getSysmanDeviceImp()->deviceGetState(&pState) != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (pState.reset) {
        pEvent = ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
        return true;
    }
    return false;
}

LinuxEventsImp::LinuxEventsImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    LinuxEventsImp *pLinuxEventsImp = new LinuxEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pLinuxEventsImp);
}

} // namespace L0

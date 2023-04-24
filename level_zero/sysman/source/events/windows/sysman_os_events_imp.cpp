/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/events/windows/sysman_os_events_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmEventsImp::eventRegister(zes_event_type_flags_t events) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmEventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    return false;
}

WddmEventsImp::WddmEventsImp(OsSysman *pOsSysman) {
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    WddmEventsImp *pWddmEventsImp = new WddmEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pWddmEventsImp);
}

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/os_events.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {

class WddmEventsImp : public OsEvents {
  public:
    bool isResetRequired(zes_event_type_flags_t &pEvent) override;
    WddmEventsImp(OsSysman *pOsSysman);
    ~WddmEventsImp() = default;

    // Don't allow copies of the WddmEventsImp object
    WddmEventsImp(const WddmEventsImp &obj) = delete;
    WddmEventsImp &operator=(const WddmEventsImp &obj) = delete;
};

bool WddmEventsImp::isResetRequired(zes_event_type_flags_t &pEvent) {
    return false;
}

WddmEventsImp::WddmEventsImp(OsSysman *pOsSysman) {
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    WddmEventsImp *pWddmEventsImp = new WddmEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pWddmEventsImp);
}

} // namespace L0

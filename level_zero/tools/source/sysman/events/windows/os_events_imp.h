/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/events/os_events.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {
class KmdSysManager;

struct EventHandler {
    HANDLE windowsHandle;
    zes_event_type_flags_t id;
    uint32_t requestId;
};

class WddmEventsImp : public OsEvents, NEO::NonCopyableOrMovableClass {
  public:
    bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) override;
    ze_result_t eventRegister(zes_event_type_flags_t events) override;
    WddmEventsImp(OsSysman *pOsSysman);
    ~WddmEventsImp() {
        CloseHandle(exitHandle);
    }

    // Don't allow copies of the WddmEventsImp object
    WddmEventsImp(const WddmEventsImp &obj) = delete;
    WddmEventsImp &operator=(const WddmEventsImp &obj) = delete;

  private:
    void registerEvents(zes_event_type_flags_t eventId, uint32_t requestId);
    void unregisterEvents();
    HANDLE exitHandle;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    std::vector<EventHandler> eventList;
};

} // namespace L0

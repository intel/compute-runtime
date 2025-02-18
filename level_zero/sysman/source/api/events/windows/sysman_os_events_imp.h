/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/events/sysman_os_events.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class KmdSysManager;
struct EventHandler {
    HANDLE windowsHandle;
    zes_event_type_flags_t id;
    uint32_t requestId;
};

class WddmEventsImp : public OsEvents, NEO::NonCopyableAndNonMovableClass {
  public:
    bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) override;
    ze_result_t eventRegister(zes_event_type_flags_t events) override;
    WddmEventsImp(OsSysman *pOsSysman);
    ~WddmEventsImp();

  private:
    void registerEvents(zes_event_type_flags_t eventId, uint32_t requestId);
    void unregisterEvents();
    HANDLE exitHandle;

  protected:
    MOCKABLE_VIRTUAL HANDLE createWddmEvent(
        LPSECURITY_ATTRIBUTES lpEventAttributes,
        BOOL bManualReset,
        BOOL bInitialState,
        LPCWSTR lpName);
    MOCKABLE_VIRTUAL void closeWddmHandle(HANDLE exitHandle);
    MOCKABLE_VIRTUAL void resetWddmEvent(HANDLE resetHandle);
    MOCKABLE_VIRTUAL void setWddmEvent(HANDLE setHandle);
    MOCKABLE_VIRTUAL uint32_t waitForMultipleWddmEvents(_In_ DWORD nCount,
                                                        _In_reads_(nCount) CONST HANDLE *lpHandles,
                                                        _In_ BOOL bWaitAll,
                                                        _In_ DWORD dwMilliseconds);
    KmdSysManager *pKmdSysManager = nullptr;
    std::vector<EventHandler> eventList;
};

static_assert(NEO::NonCopyableAndNonMovable<WddmEventsImp>);

} // namespace Sysman
} // namespace L0

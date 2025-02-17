/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "events.h"
#include "os_events.h"

#include <mutex>

namespace L0 {

class EventsImp : public Events, NEO::NonCopyableAndNonMovableClass {
  public:
    void init() override;
    ze_result_t eventRegister(zes_event_type_flags_t events) override;
    bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) override;
    OsEvents *pOsEvents = nullptr;

    EventsImp() = default;
    EventsImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~EventsImp() override;

  private:
    OsSysman *pOsSysman = nullptr;
    std::once_flag initEventsOnce;
    void initEvents();
};

} // namespace L0

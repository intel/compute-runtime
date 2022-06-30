/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "events.h"
#include "os_events.h"

namespace L0 {

class EventsImp : public Events, NEO::NonCopyableOrMovableClass {
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
};

} // namespace L0

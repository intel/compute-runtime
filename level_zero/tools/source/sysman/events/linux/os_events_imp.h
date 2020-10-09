/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/events/os_events.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxEventsImp : public OsEvents, NEO::NonCopyableOrMovableClass {
  public:
    bool isResetRequired(zes_event_type_flags_t &pEvent) override;
    LinuxEventsImp() = default;
    LinuxEventsImp(OsSysman *pOsSysman);
    ~LinuxEventsImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

} // namespace L0

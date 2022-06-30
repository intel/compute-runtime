/*
 * Copyright (C) 2020-2021 Intel Corporation
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
    bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) override;
    ze_result_t eventRegister(zes_event_type_flags_t events) override;
    LinuxEventsImp() = default;
    LinuxEventsImp(OsSysman *pOsSysman);
    ~LinuxEventsImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    void getPciIdPathTag();
    zes_mem_health_t currentMemHealth();
    bool isResetRequired(zes_event_type_flags_t &pEvent);
    bool checkDeviceDetachEvent(zes_event_type_flags_t &pEvent);
    bool checkDeviceAttachEvent(zes_event_type_flags_t &pEvent);
    bool checkIfMemHealthChanged(zes_event_type_flags_t &pEvent);
    std::string pciIdPathTag;
    zes_mem_health_t memHealthAtEventRegister = ZES_MEM_HEALTH_UNKNOWN;

  private:
    FsAccess *pFsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    static const std::string varFs;
    static const std::string detachEvent;
    static const std::string attachEvent;
    zes_event_type_flags_t registeredEvents = 0;
};

} // namespace L0

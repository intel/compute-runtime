/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/events/os_events.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/linux/udev/udev_lib.h"

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
    bool isResetRequired(void *dev, zes_event_type_flags_t &pEvent);
    bool checkDeviceDetachEvent(zes_event_type_flags_t &pEvent);
    bool checkDeviceAttachEvent(zes_event_type_flags_t &pEvent);
    bool checkIfMemHealthChanged(void *dev, zes_event_type_flags_t &pEvent);
    bool checkIfFabricPortStatusChanged(void *dev, zes_event_type_flags_t &pEvent);
    bool checkRasEvent(zes_event_type_flags_t &pEvent);
    ze_result_t readFabricDeviceStats(const std::string &devicePciPath, struct stat &iafStat);
    bool listenSystemEvents(zes_event_type_flags_t &pEvent, uint64_t timeout);
    uint32_t fabricEventTrackAtRegister = 0;
    L0::UdevLib *pUdevLib = nullptr;
    zes_event_type_flags_t registeredEvents = 0;

  private:
    FsAccess *pFsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    std::string action;
    static const std::string add;
    static const std::string remove;
    static const std::string change;
    static const std::string unbind;
    static const std::string bind;
};

} // namespace L0

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/linux/udev/udev_lib.h"
#include "level_zero/tools/source/sysman/os_sysman_driver.h"

namespace L0 {

class LinuxEventsUtil;
struct SysmanDeviceImp;

class LinuxSysmanDriverImp : public OsSysmanDriver, NEO::NonCopyableAndNonMovableClass {
  public:
    LinuxSysmanDriverImp();
    ~LinuxSysmanDriverImp() override;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;
    void eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice);
    L0::UdevLib *getUdevLibHandle();

  protected:
    L0::UdevLib *pUdevLib = nullptr;
    L0::LinuxEventsUtil *pLinuxEventsUtil = nullptr;
};

void __attribute__((destructor)) osSysmanDriverDestructor();

} // namespace L0

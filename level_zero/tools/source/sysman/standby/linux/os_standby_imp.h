/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/standby/os_standby.h"
#include "sysman/standby/standby_imp.h"

namespace L0 {

class LinuxStandbyImp : public OsStandby, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getMode(zes_standby_promo_mode_t &mode) override;
    ze_result_t setMode(zes_standby_promo_mode_t mode) override;
    ze_result_t osStandbyGetProperties(zes_standby_properties_t &properties) override;

    bool isStandbySupported(void) override;

    LinuxStandbyImp() = default;
    LinuxStandbyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    ~LinuxStandbyImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    std::string standbyModeFile;
    static const int standbyModeDefault = 1;
    static const int standbyModeNever = 0;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    void init();
};

} // namespace L0

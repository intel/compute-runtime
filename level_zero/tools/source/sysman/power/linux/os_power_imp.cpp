/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/power/os_power.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxPowerImp : public OsPower, public NEO::NonCopyableClass {
  public:
    LinuxPowerImp(OsSysman *pOsSysman);
    ~LinuxPowerImp() override = default;

  private:
    SysfsAccess *pSysfsAccess = nullptr;
};

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0
/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/temperature/os_temperature.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxTemperatureImp : public OsTemperature, public NEO::NonCopyableClass {
  public:
    LinuxTemperatureImp(OsSysman *pOsSysman);
    ~LinuxTemperatureImp() override = default;

  private:
    SysfsAccess *pSysfsAccess = nullptr;
};

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    LinuxTemperatureImp *pLinuxTemperatureImp = new LinuxTemperatureImp(pOsSysman);
    return static_cast<OsTemperature *>(pLinuxTemperatureImp);
}

} // namespace L0
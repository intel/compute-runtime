/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxRasImp : public OsRas, public NEO::NonCopyableClass {
  public:
    LinuxRasImp(OsSysman *pOsSysman);
    ~LinuxRasImp() override = default;

  private:
    SysfsAccess *pSysfsAccess = nullptr;
};

LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0

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
    ze_result_t getCounterValues(zet_ras_details_t *pDetails) override;

  private:
    FsAccess *pFsAccess = nullptr;
};

ze_result_t LinuxRasImp::getCounterValues(zet_ras_details_t *pDetails) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0

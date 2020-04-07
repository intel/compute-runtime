/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/engine/os_engine.h"
#include "sysman/linux/fs_access.h"
#include "sysman/linux/os_sysman_imp.h"
namespace L0 {

class LinuxEngineImp : public OsEngine, public NEO::NonCopyableClass {
  public:
    ze_result_t getActiveTime(uint64_t &activeTime) override;

    LinuxEngineImp(OsSysman *pOsSysman);
    ~LinuxEngineImp() override = default;

  private:
    SysfsAccess *pSysfsAccess;
};

ze_result_t LinuxEngineImp::getActiveTime(uint64_t &activeTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}
OsEngine *OsEngine::create(OsSysman *pOsSysman) {
    LinuxEngineImp *pLinuxEngineImp = new LinuxEngineImp(pOsSysman);
    return static_cast<OsEngine *>(pLinuxEngineImp);
}

} // namespace L0

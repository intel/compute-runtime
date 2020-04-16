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
    ze_result_t getEngineGroup(zet_engine_group_t &engineGroup) override;

    LinuxEngineImp(OsSysman *pOsSysman);
    ~LinuxEngineImp() override = default;

  private:
    SysfsAccess *pSysfsAccess;
    static const std::string mediaEngineGroupFile;
    static const std::string mediaEngineGroupName;
};
const std::string LinuxEngineImp::mediaEngineGroupFile("vcs0/name");
const std::string LinuxEngineImp::mediaEngineGroupName("vcs0");
ze_result_t LinuxEngineImp::getActiveTime(uint64_t &activeTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxEngineImp::getEngineGroup(zet_engine_group_t &engineGroup) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(mediaEngineGroupFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    if (strVal.compare(mediaEngineGroupName) == 0) {
        engineGroup = ZET_ENGINE_GROUP_MEDIA_ALL;
    } else {
        engineGroup = ZET_ENGINE_GROUP_ALL;
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return result;
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

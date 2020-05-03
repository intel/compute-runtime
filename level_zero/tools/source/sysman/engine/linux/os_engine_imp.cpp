/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
namespace L0 {

const std::string LinuxEngineImp::computeEngineGroupFile("engine/rcs0/name");
const std::string LinuxEngineImp::computeEngineGroupName("rcs0");
ze_result_t LinuxEngineImp::getActiveTime(uint64_t &activeTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxEngineImp::getEngineGroup(zet_engine_group_t &engineGroup) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(computeEngineGroupFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    if (strVal.compare(computeEngineGroupName) == 0) {
        engineGroup = ZET_ENGINE_GROUP_COMPUTE_ALL;
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

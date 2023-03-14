/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/engine/windows/os_engine_imp.h"

namespace L0 {
namespace Sysman {

OsEngine *OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) {
    WddmEngineImp *pWddmEngineImp = new WddmEngineImp(pOsSysman, engineType, engineInstance, subDeviceId);
    return static_cast<OsEngine *>(pWddmEngineImp);
}

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0

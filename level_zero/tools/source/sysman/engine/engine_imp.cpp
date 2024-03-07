/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

namespace L0 {

ze_result_t EngineImp::engineGetActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) {
    return pOsEngine->getActivityExt(pCount, pStats);
}

ze_result_t EngineImp::engineGetActivity(zes_engine_stats_t *pStats) {
    return pOsEngine->getActivity(pStats);
}

ze_result_t EngineImp::engineGetProperties(zes_engine_properties_t *pProperties) {
    *pProperties = engineProperties;
    return ZE_RESULT_SUCCESS;
}

void EngineImp::init() {
    initStatus = pOsEngine->isEngineModuleSupported();
    if (initStatus == ZE_RESULT_SUCCESS) {
        pOsEngine->getProperties(engineProperties);
    }
}

EngineImp::EngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubdevice) {
    pOsEngine = OsEngine::create(pOsSysman, engineType, engineInstance, subDeviceId, onSubdevice);
    init();
}

EngineImp::~EngineImp() = default;

} // namespace L0

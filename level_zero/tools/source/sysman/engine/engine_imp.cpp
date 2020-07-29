/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t EngineImp::engineGetActivity(zes_engine_stats_t *pStats) {
    ze_result_t result = pOsEngine->getActiveTime(pStats->activeTime);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    return pOsEngine->getTimeStamp(pStats->timestamp);
}

ze_result_t EngineImp::engineGetProperties(zes_engine_properties_t *pProperties) {
    *pProperties = engineProperties;
    return ZE_RESULT_SUCCESS;
}

void EngineImp::init(zes_engine_group_t type) {
    if (pOsEngine->getEngineGroup(type) == ZE_RESULT_SUCCESS) {
        this->initSuccess = true;
    } else {
        this->initSuccess = false;
    }
    engineProperties.type = type;
    engineProperties.onSubdevice = false;
    engineProperties.subdeviceId = 0;
}

EngineImp::EngineImp(OsSysman *pOsSysman, zes_engine_group_t type) {
    pOsEngine = OsEngine::create(pOsSysman);
    init(type);
}

EngineImp::~EngineImp() {
    if (nullptr != pOsEngine) {
        delete pOsEngine;
        pOsEngine = nullptr;
    }
}

} // namespace L0

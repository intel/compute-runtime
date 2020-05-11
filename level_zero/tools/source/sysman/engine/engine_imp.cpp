/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

#include "level_zero/core/source/device/device.h"

#include <chrono>

namespace L0 {

void engineGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}
ze_result_t EngineImp::engineGetActivity(zet_engine_stats_t *pStats) {

    ze_result_t result;

    result = pOsEngine->getActiveTime(pStats->activeTime);

    engineGetTimestamp(pStats->timestamp);

    return result;
}

void EngineImp::init() {
    zet_engine_group_t engineGroup = ZET_ENGINE_GROUP_ALL;
    if (pOsEngine->getEngineGroup(engineGroup) == ZE_RESULT_SUCCESS) {
        this->initSuccess = true;
    } else {
        this->initSuccess = false;
    }
    engineProperties.type = engineGroup;
    engineProperties.onSubdevice = false;
    engineProperties.subdeviceId = 0;
}
ze_result_t EngineImp::engineGetProperties(zet_engine_properties_t *pProperties) {
    *pProperties = engineProperties;
    return ZE_RESULT_SUCCESS;
}
EngineImp::EngineImp(OsSysman *pOsSysman) {
    pOsEngine = OsEngine::create(pOsSysman);

    init();
}

EngineImp::~EngineImp() {
    if (nullptr != pOsEngine) {
        delete pOsEngine;
        pOsEngine = nullptr;
    }
}

} // namespace L0

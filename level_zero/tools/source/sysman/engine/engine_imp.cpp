/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t engineGetTimestamp(uint64_t &timestamp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t EngineImp::engineGetActivity(zet_engine_stats_t *pStats) {

    ze_result_t result;

    result = pOsEngine->getActiveTime(pStats->activeTime);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = engineGetTimestamp(pStats->timestamp);

    return result;
}

ze_result_t EngineImp::engineGetProperties(zet_engine_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
EngineImp::EngineImp(OsSysman *pOsSysman) {
    pOsEngine = OsEngine::create(pOsSysman);
}

EngineImp::~EngineImp() {
    if (nullptr != pOsEngine) {
        delete pOsEngine;
    }
}

} // namespace L0

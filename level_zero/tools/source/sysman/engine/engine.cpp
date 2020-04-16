/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine.h"

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

namespace L0 {

EngineHandleContext::~EngineHandleContext() {
    for (Engine *pEngine : handleList) {
        delete pEngine;
    }
}

ze_result_t EngineHandleContext::init() {
    Engine *pEngine = new EngineImp(pOsSysman);
    if (pEngine->initSuccess == true) {
        handleList.push_back(pEngine);
    } else {
        delete pEngine;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t EngineHandleContext::engineGet(uint32_t *pCount, zet_sysman_engine_handle_t *phEngine) {
    if (nullptr == phEngine) {
        *pCount = static_cast<uint32_t>(handleList.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Engine *engine : handleList) {
        if (i >= *pCount) {
            break;
        }
        phEngine[i++] = engine->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

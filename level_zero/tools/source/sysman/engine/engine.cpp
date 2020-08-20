/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

namespace L0 {

EngineHandleContext::~EngineHandleContext() {
    for (Engine *pEngine : handleList) {
        delete pEngine;
    }
}

void EngineHandleContext::createHandle(zes_engine_group_t type) {
    Engine *pEngine = new EngineImp(pOsSysman, type);
    if (pEngine->initSuccess == true) {
        handleList.push_back(pEngine);
    } else {
        delete pEngine;
    }
}

void EngineHandleContext::init() {
    createHandle(ZES_ENGINE_GROUP_COMPUTE_ALL);
}

ze_result_t EngineHandleContext::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phEngine) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phEngine[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

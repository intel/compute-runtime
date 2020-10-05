/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/engine/engine_imp.h"
class OsEngine;
namespace L0 {

EngineHandleContext::EngineHandleContext(OsSysman *pOsSysman) {
    this->pOsSysman = pOsSysman;
}

EngineHandleContext::~EngineHandleContext() {
    releaseEngines();
}

void EngineHandleContext::createHandle(zes_engine_group_t engineType, uint32_t engineInstance) {
    Engine *pEngine = new EngineImp(pOsSysman, engineType, engineInstance);
    handleList.push_back(pEngine);
}

void EngineHandleContext::init() {
    std::multimap<zes_engine_group_t, uint32_t> engineGroupInstance = {};
    OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman);
    for (auto itr = engineGroupInstance.begin(); itr != engineGroupInstance.end(); ++itr) {
        createHandle(itr->first, itr->second);
    }
}

void EngineHandleContext::releaseEngines() {
    for (Engine *pEngine : handleList) {
        delete pEngine;
    }
    handleList.clear();
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

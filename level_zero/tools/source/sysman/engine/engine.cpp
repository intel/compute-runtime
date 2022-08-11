/*
 * Copyright (C) 2020-2022 Intel Corporation
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

void EngineHandleContext::createHandle(zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId) {
    Engine *pEngine = new EngineImp(pOsSysman, engineType, engineInstance, subDeviceId);
    if (pEngine->initSuccess == true) {
        handleList.push_back(pEngine);
    } else {
        delete pEngine;
    }
}

void EngineHandleContext::init() {
    std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> engineGroupInstance = {}; //set contains pair of engine group and struct containing engine instance and subdeviceId
    OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman);
    for (auto itr = engineGroupInstance.begin(); itr != engineGroupInstance.end(); ++itr) {
        createHandle(itr->first, itr->second.first, itr->second.second);
    }
}

void EngineHandleContext::releaseEngines() {
    for (Engine *pEngine : handleList) {
        delete pEngine;
    }
    handleList.clear();
}

ze_result_t EngineHandleContext::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    std::call_once(initEngineOnce, [this]() {
        this->init();
        this->engineInitDone = true;
    });
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

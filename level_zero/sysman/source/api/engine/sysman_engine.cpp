/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/engine/sysman_engine.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/engine/sysman_engine_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"

namespace L0 {
namespace Sysman {

EngineHandleContext::EngineHandleContext(OsSysman *pOsSysman) {
    this->pOsSysman = pOsSysman;
}

EngineHandleContext::~EngineHandleContext() {
    releaseEngines();
}

void EngineHandleContext::createHandle(zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubdevice) {
    std::unique_ptr<Engine> pEngine = std::make_unique<EngineImp>(pOsSysman, engineType, engineInstance, tileId, onSubdevice);
    if (pEngine->initSuccess == true) {
        handleList.push_back(std::move(pEngine));
    }
}

void EngineHandleContext::init(uint32_t subDeviceCount) {
    std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> engineGroupInstance = {}; // set contains pair of engine group and struct containing engine instance and gtId
    deviceEngineInitStatus = OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman);

    if (deviceEngineInitStatus != ZE_RESULT_SUCCESS) {
        return;
    }

    for (auto itr = engineGroupInstance.begin(); itr != engineGroupInstance.end(); ++itr) {
        const auto isSubDevice = subDeviceCount > 0;
        createHandle(itr->first, itr->second.first, itr->second.second, isSubDevice);
    }

    OsEngine::initGroupEngineHandleGroupFd(pOsSysman);
}

void EngineHandleContext::releaseEngines() {
    OsEngine::closeFdsForGroupEngineHandles(pOsSysman);
    handleList.clear();
}

ze_result_t EngineHandleContext::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    std::call_once(initEngineOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
        this->engineInitDone = true;
    });

    if (deviceEngineInitStatus != ZE_RESULT_SUCCESS) {
        return deviceEngineInitStatus;
    }

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

} // namespace Sysman
} // namespace L0

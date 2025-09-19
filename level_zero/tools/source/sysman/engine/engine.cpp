/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/engine/engine_imp.h"
#include "level_zero/tools/source/sysman/os_sysman.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
class OsEngine;
namespace L0 {

EngineHandleContext::EngineHandleContext(OsSysman *pOsSysman) {
    this->pOsSysman = pOsSysman;
}

EngineHandleContext::~EngineHandleContext() {
    releaseEngines();
}

void EngineHandleContext::createHandle(zes_engine_group_t engineType, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubdevice) {
    std::unique_ptr<Engine> pEngine = std::make_unique<EngineImp>(pOsSysman, engineType, engineInstance, subDeviceId, onSubdevice);
    // Only store error for all engines in device in case of dependencies unavailable.
    deviceEngineInitStatus = pEngine->initStatus != ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE ? deviceEngineInitStatus : pEngine->initStatus;
    if (pEngine->initStatus == ZE_RESULT_SUCCESS) {
        handleList.push_back(std::move(pEngine));
    }
}

void EngineHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> engineGroupInstance = {}; // set contains pair of engine group and struct containing engine instance and subdeviceId
    OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman);
    for (auto itr = engineGroupInstance.begin(); itr != engineGroupInstance.end(); ++itr) {
        for (const auto &deviceHandle : deviceHandles) {
            uint32_t subDeviceId = 0;
            ze_bool_t onSubdevice = false;
            SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subDeviceId, onSubdevice, true);
            if (subDeviceId == itr->second.second) {
                createHandle(itr->first, itr->second.first, subDeviceId, onSubdevice);
            }
        }
    }
}

void EngineHandleContext::releaseEngines() {
    handleList.clear();
}

ze_result_t EngineHandleContext::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    std::call_once(initEngineOnce, [this]() {
        this->init(pOsSysman->getDeviceHandles());
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

} // namespace L0

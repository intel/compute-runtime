/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    Engine *pEngine = new EngineImp(pOsSysman, engineType, engineInstance, subDeviceId, onSubdevice);
    if (pEngine->initSuccess == true) {
        handleList.push_back(pEngine);
    } else {
        delete pEngine;
    }
}

void EngineHandleContext::init(std::vector<ze_device_handle_t> &deviceHandles) {
    std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> engineGroupInstance = {}; //set contains pair of engine group and struct containing engine instance and subdeviceId
    OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman);
    for (auto itr = engineGroupInstance.begin(); itr != engineGroupInstance.end(); ++itr) {
        for (const auto &deviceHandle : deviceHandles) {
            uint32_t subDeviceId = 0;
            ze_bool_t onSubdevice = false;
            SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subDeviceId, onSubdevice);
            if (subDeviceId == itr->second.second) {
                createHandle(itr->first, itr->second.first, subDeviceId, onSubdevice);
            }
        }
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
        this->init(pOsSysman->getDeviceHandles());
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

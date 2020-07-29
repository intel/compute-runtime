/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/temperature.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"

namespace L0 {

TemperatureHandleContext::~TemperatureHandleContext() {
    for (Temperature *pTemperature : handleList) {
        delete pTemperature;
    }
}

void TemperatureHandleContext::createHandle(zet_temp_sensors_t type) {
    Temperature *pTemperature = new TemperatureImp(pOsSysman, type);
    if (pTemperature->initSuccess == true) {
        handleList.push_back(pTemperature);
    } else {
        delete pTemperature;
    }
}

void TemperatureHandleContext::init() {
    createHandle(ZET_TEMP_SENSORS_GLOBAL);
    createHandle(ZET_TEMP_SENSORS_GPU);
}

ze_result_t TemperatureHandleContext::temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) {
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phTemperature) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phTemperature[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

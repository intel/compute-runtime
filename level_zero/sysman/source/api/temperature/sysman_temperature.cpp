/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {

TemperatureHandleContext::~TemperatureHandleContext() {
    releaseTemperatureHandles();
}

void TemperatureHandleContext::releaseTemperatureHandles() {
    handleList.clear();
}

void TemperatureHandleContext::createHandle(bool onSubdevice, uint32_t subDeviceId, zes_temp_sensors_t type) {
    std::unique_ptr<Temperature> pTemperature = std::make_unique<TemperatureImp>(pOsSysman, onSubdevice, subDeviceId, type);
    if (pTemperature->initSuccess == true) {
        handleList.push_back(std::move(pTemperature));
    }
}

ze_result_t TemperatureHandleContext::init(uint32_t subDeviceCount) {

    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createHandle(true, subDeviceId, ZES_TEMP_SENSORS_GLOBAL);
            createHandle(true, subDeviceId, ZES_TEMP_SENSORS_GPU);
            createHandle(true, subDeviceId, ZES_TEMP_SENSORS_MEMORY);
        }
    } else {
        createHandle(false, 0, ZES_TEMP_SENSORS_GLOBAL);
        createHandle(false, 0, ZES_TEMP_SENSORS_GPU);
        createHandle(false, 0, ZES_TEMP_SENSORS_MEMORY);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t TemperatureHandleContext::temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) {
    std::call_once(initTemperatureOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
        this->tempInitDone = true;
    });
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

} // namespace Sysman
} // namespace L0

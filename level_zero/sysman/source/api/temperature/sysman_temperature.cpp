/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

void TemperatureHandleContext::createHandle(bool onSubdevice, uint32_t subDeviceId, zes_temp_sensors_t type, uint32_t sensorIndex) {
    std::unique_ptr<Temperature> pTemperature = std::make_unique<TemperatureImp>(pOsSysman, onSubdevice, subDeviceId, type, sensorIndex);
    if (pTemperature->initSuccess == true) {
        handleList.push_back(std::move(pTemperature));
    }
}

ze_result_t TemperatureHandleContext::init(uint32_t subDeviceCount) {

    std::map<zes_temp_sensors_t, uint32_t> supportedSensorTypeMap = {};
    OsTemperature::getSupportedSensors(pOsSysman, supportedSensorTypeMap);

    // Lambda to create temperature handles for a given device/sub-device
    auto createTemperatureHandles = [this, &supportedSensorTypeMap](bool onSubdevice, uint32_t subDeviceId) {
        for (const auto &[sensorType, sensorCount] : supportedSensorTypeMap) {
            for (uint32_t sensorIndex = 0; sensorIndex < sensorCount; sensorIndex++) {
                createHandle(onSubdevice, subDeviceId, sensorType, sensorIndex);
            }
        }
    };

    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createTemperatureHandles(true, subDeviceId);
        }
    } else {
        createTemperatureHandles(false, 0);
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

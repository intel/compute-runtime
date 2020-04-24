/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/temperature.h"

#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"

namespace L0 {

TemperatureHandleContext::~TemperatureHandleContext() {
    for (Temperature *pTemperature : handleList) {
        delete pTemperature;
    }
}

void TemperatureHandleContext::init() {
    Temperature *pTemperature = new TemperatureImp(pOsSysman);
    if (pTemperature->initSuccess == true) {
        handleList.push_back(pTemperature);
    } else {
        delete pTemperature;
    }
}

ze_result_t TemperatureHandleContext::temperatureGet(uint32_t *pCount, zet_sysman_temp_handle_t *phTemperature) {
    if (nullptr == phTemperature) {
        *pCount = static_cast<uint32_t>(handleList.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (Temperature *temperature : handleList) {
        if (i >= *pCount) {
            break;
        }
        phTemperature[i++] = temperature->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0

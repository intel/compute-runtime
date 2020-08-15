/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/fan/windows/os_fan_imp.h"

namespace L0 {

void GetOsTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t WddmFanImp::getProperties(zes_fan_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    request.dataSize = sizeof(uint32_t);

    uint32_t FanPoints = 2;
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &FanPoints, sizeof(uint32_t));

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    pProperties->canControl = (status == ZE_RESULT_SUCCESS);
    request.dataSize = 0;
    memset(request.dataBuffer, request.dataSize, sizeof(request.dataBuffer));

    request.commandId = KmdSysman::Command::Get;
    request.requestId = KmdSysman::Requests::Fans::MaxFanControlPointsSupported;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&FanPoints, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pProperties->maxPoints = static_cast<int32_t>(FanPoints);
    pProperties->maxRPM = -1;
    pProperties->supportedModes = 1;
    pProperties->supportedUnits = 1;

    return ZE_RESULT_SUCCESS;
}
ze_result_t WddmFanImp::getConfig(zes_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFanImp::setDefaultMode() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFanImp::setFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFanImp::setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFanImp::getState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    if (units == ZES_FAN_SPEED_UNITS_PERCENT) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;
    uint64_t currentTS = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanSpeed;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    GetOsTimestamp(currentTS);
    uint64_t diffTime = currentTS - prevTS;
    uint32_t diffPulses = value - prevPulses;

    double pulsesPerTime = static_cast<double>(diffPulses) / static_cast<double>(diffTime);
    pulsesPerTime *= 1000000.0;
    pulsesPerTime *= 60.0;

    prevTS = currentTS;
    prevPulses = value;

    *pSpeed = static_cast<int32_t>(pulsesPerTime);

    return status;
}

bool WddmFanImp::isFanModuleSupported() {
    return true;
}

WddmFanImp::WddmFanImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsFan *OsFan::create(OsSysman *pOsSysman) {
    WddmFanImp *pWddmFanImp = new WddmFanImp(pOsSysman);
    return static_cast<OsFan *>(pWddmFanImp);
}

} // namespace L0

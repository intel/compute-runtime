/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/fan/windows/os_fan_imp.h"

namespace L0 {

struct FanPoint {
    union {
        struct {
            int32_t temperatureDegreesCelsius : 16;
            int32_t fanSpeedPercent : 16;
        };
        int32_t data;
    };
};

ze_result_t WddmFanImp::getProperties(zes_fan_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    request.dataSize = sizeof(uint32_t);

    uint32_t FanPoints = 2;
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &FanPoints, sizeof(uint32_t));

    vRequests.push_back(request);

    request.dataSize = 0;
    memset(request.dataBuffer, request.dataSize, sizeof(request.dataBuffer));
    request.commandId = KmdSysman::Command::Get;
    request.requestId = KmdSysman::Requests::Fans::MaxFanControlPointsSupported;

    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    pProperties->canControl = (vResponses[0].returnCode == KmdSysman::Success);

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&FanPoints, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        pProperties->maxPoints = maxPoints = static_cast<int32_t>(FanPoints);
    }
    pProperties->maxRPM = -1;
    pProperties->supportedModes = zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE;
    pProperties->supportedUnits = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;

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
    if (pSpeedTable->numPoints == 0 || pSpeedTable->numPoints > maxPoints) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (int32_t i = 0; i < pSpeedTable->numPoints; i++) {
        if (pSpeedTable->table[i].speed.units == zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM) {
            return ze_result_t::ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    uint32_t value = pSpeedTable->numPoints;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    request.dataSize = sizeof(uint32_t);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Fans::CurrentFanPoint;
    for (int32_t i = 0; i < pSpeedTable->numPoints; i++) {
        FanPoint point = {};
        point.fanSpeedPercent = pSpeedTable->table[i].speed.speed;
        point.temperatureDegreesCelsius = pSpeedTable->table[i].temperature;
        value = point.data;
        memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
        vRequests.push_back(request);
    }

    return pKmdSysManager->requestMultiple(vRequests, vResponses);
    ;
}

ze_result_t WddmFanImp::getState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    if (units == ZES_FAN_SPEED_UNITS_PERCENT) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanSpeed;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    *pSpeed = static_cast<int32_t>(value);

    return status;
}

bool WddmFanImp::isFanModuleSupported() {
    KmdSysman::RequestProperty request = {};
    KmdSysman::ResponseProperty response = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanSpeed;

    return (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS);
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

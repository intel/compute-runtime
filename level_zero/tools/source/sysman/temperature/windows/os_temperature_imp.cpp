/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/temperature/windows/os_temperature_imp.h"

namespace L0 {

ze_result_t WddmTemperatureImp::getProperties(zes_temp_properties_t *pProperties) {
    uint32_t value = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    pProperties->type = this->type;
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;

    switch (this->type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainPackage;
        break;
    case ZES_TEMP_SENSORS_GPU:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainDGPU;
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainPackage;
        break;
    default:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    request.requestId = KmdSysman::Requests::Temperature::TempCriticalEventSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Temperature::TempThreshold1EventSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Temperature::TempThreshold2EventSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Temperature::MaxTempSupported;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->isCriticalTempSupported, sizeof(ze_bool_t), vResponses[0].dataBuffer, sizeof(ze_bool_t));
    }

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->isThreshold1Supported, sizeof(ze_bool_t), vResponses[1].dataBuffer, sizeof(ze_bool_t));
    }

    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->isThreshold2Supported, sizeof(ze_bool_t), vResponses[2].dataBuffer, sizeof(ze_bool_t));
    }

    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        pProperties->maxTemperature = static_cast<double>(value);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmTemperatureImp::getSensorTemperature(double *pTemperature) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;
    request.requestId = KmdSysman::Requests::Temperature::CurrentTemperature;

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainPackage;
        break;
    case ZES_TEMP_SENSORS_GPU:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainDGPU;
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        request.paramInfo = KmdSysman::TemperatureDomainsType::TemperatureDomainHBM;
        break;
    default:
        *pTemperature = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pTemperature = static_cast<double>(value);

    return status;
}

bool WddmTemperatureImp::isTempModuleSupported() {
    if ((type == ZES_TEMP_SENSORS_GLOBAL_MIN) || (type == ZES_TEMP_SENSORS_GPU_MIN)) {
        return false;
    }
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.paramInfo = static_cast<uint32_t>(type);
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::TemperatureComponent;
    request.requestId = KmdSysman::Requests::Temperature::CurrentTemperature;

    return (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS);
}

void WddmTemperatureImp::setSensorType(zes_temp_sensors_t sensorType) {
    type = sensorType;
}

WddmTemperatureImp::WddmTemperatureImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

std::unique_ptr<OsTemperature> OsTemperature::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType) {
    std::unique_ptr<WddmTemperatureImp> pWddmTemperatureImp = std::make_unique<WddmTemperatureImp>(pOsSysman);
    pWddmTemperatureImp->setSensorType(sensorType);
    return std::move(pWddmTemperatureImp);
}

} // namespace L0
/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/temperature/windows/os_temperature_imp.h"

namespace L0 {

ze_result_t WddmTemperatureImp::getProperties(zes_temp_properties_t *pProperties) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

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

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->isCriticalTempSupported, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

    request.requestId = KmdSysman::Requests::Temperature::TempThreshold1EventSupported;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->isThreshold1Supported, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

    request.requestId = KmdSysman::Requests::Temperature::TempThreshold2EventSupported;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->isThreshold2Supported, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

    request.requestId = KmdSysman::Requests::Temperature::MaxTempSupported;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pProperties->maxTemperature = static_cast<double>(value);

    return status;
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
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

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

OsTemperature *OsTemperature::create(OsSysman *pOsSysman, zes_temp_sensors_t sensorType) {
    WddmTemperatureImp *pWddmTemperatureImp = new WddmTemperatureImp(pOsSysman);
    pWddmTemperatureImp->setSensorType(sensorType);
    return static_cast<OsTemperature *>(pWddmTemperatureImp);
}

} // namespace L0
/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/power/windows/os_power_imp.h"

namespace L0 {

ze_result_t WddmPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::EnergyThresholdSupported;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->canControl, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));
    memcpy_s(&pProperties->isEnergyThresholdSupported, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

    request.requestId = KmdSysman::Requests::Power::TdpDefault;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->defaultLimit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    request.requestId = KmdSysman::Requests::Power::MinPowerLimitDefault;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->minLimit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    request.requestId = KmdSysman::Requests::Power::MaxPowerLimitDefault;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&pProperties->maxLimit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    return status;
}

ze_result_t WddmPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    uint32_t energyUnits = 0;
    uint32_t timestampFrequency = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::EnergyCounterUnits;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&energyUnits, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    request.requestId = KmdSysman::Requests::Power::CurrentEnergyCounter;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t valueCounter = 0;
    uint64_t valueTimeStamp = 0;
    memcpy_s(&valueCounter, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    uint32_t conversionUnit = (1 << energyUnits);
    double valueConverted = static_cast<double>(valueCounter) / static_cast<double>(conversionUnit);
    valueConverted *= static_cast<double>(convertJouleToMicroJoule);
    pEnergy->energy = static_cast<uint64_t>(valueConverted);
    memcpy_s(&valueTimeStamp, sizeof(uint64_t), (response.dataBuffer + sizeof(uint32_t)), sizeof(uint64_t));

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;
    request.requestId = KmdSysman::Requests::Activity::TimestampFrequency;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&timestampFrequency, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    double timeFactor = 1.0 / static_cast<double>(timestampFrequency);
    timeFactor = static_cast<double>(valueTimeStamp) * timeFactor;
    timeFactor *= static_cast<double>(microFacor);
    pEnergy->timestamp = static_cast<uint64_t>(timeFactor);

    return status;
}

ze_result_t WddmPowerImp::getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;

    if (pSustained) {
        memset(pSustained, 0, sizeof(zes_power_sustained_limit_t));

        request.requestId = KmdSysman::Requests::Power::PowerLimit1Enabled;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pSustained->enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pSustained->power, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1Tau;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pSustained->interval, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    }

    if (pBurst) {
        memset(pBurst, 0, sizeof(zes_power_burst_limit_t));

        request.requestId = KmdSysman::Requests::Power::PowerLimit2Enabled;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pBurst->enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit2;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pBurst->power, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    }

    if (pPeak) {
        memset(pPeak, 0, sizeof(zes_power_peak_limit_t));

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Ac;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pPeak->powerAC, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Dc;

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        memcpy_s(&pPeak->powerDC, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    }

    return status;
}

ze_result_t WddmPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.dataSize = sizeof(uint32_t);

    if (pSustained) {
        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;

        memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained->power, sizeof(uint32_t));

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1Tau;

        memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained->interval, sizeof(uint32_t));

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }

    if (pBurst) {
        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit2;

        memcpy_s(request.dataBuffer, sizeof(uint32_t), &pBurst->power, sizeof(uint32_t));

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }

    if (pPeak) {
        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Ac;

        memcpy_s(request.dataBuffer, sizeof(uint32_t), &pPeak->powerAC, sizeof(uint32_t));

        status = pKmdSysManager->requestSingle(request, response);

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Dc;

        memcpy_s(request.dataBuffer, sizeof(uint32_t), &pPeak->powerDC, sizeof(uint32_t));

        status = pKmdSysManager->requestSingle(request, response);
    }

    return status;
}

ze_result_t WddmPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    pThreshold->processId = 0;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentEnergyThreshold;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memset(pThreshold, 0, sizeof(zes_energy_threshold_t));

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pThreshold->threshold = static_cast<double>(value);
    pThreshold->enable = true;

    return status;
}

ze_result_t WddmPowerImp::setEnergyThreshold(double threshold) {
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentEnergyThreshold;
    request.dataSize = sizeof(uint32_t);

    uint32_t value = static_cast<uint32_t>(threshold);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));

    return pKmdSysManager->requestSingle(request, response);
}

bool WddmPowerImp::isPowerModuleSupported() {
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::PowerLimit1Enabled;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    uint32_t enabled = 0;
    memcpy_s(&enabled, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    return ((status == ZE_RESULT_SUCCESS) && (enabled));
}

WddmPowerImp::WddmPowerImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    WddmPowerImp *pWddmPowerImp = new WddmPowerImp(pOsSysman);
    return static_cast<OsPower *>(pWddmPowerImp);
}

} // namespace L0

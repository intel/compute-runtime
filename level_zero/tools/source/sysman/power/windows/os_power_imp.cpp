/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/power/windows/os_power_imp.h"

namespace L0 {

ze_result_t WddmPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::EnergyThresholdSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::TdpDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::MinPowerLimitDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::MaxPowerLimitDefault;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->canControl, sizeof(ze_bool_t), vResponses[0].dataBuffer, sizeof(ze_bool_t));
        memcpy_s(&pProperties->isEnergyThresholdSupported, sizeof(ze_bool_t), vResponses[0].dataBuffer, sizeof(ze_bool_t));
    }

    pProperties->defaultLimit = -1;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->defaultLimit, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
    }

    pProperties->minLimit = -1;
    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->minLimit, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
    }

    pProperties->maxLimit = -1;
    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&pProperties->maxLimit, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    uint32_t energyUnits = 0;
    uint32_t timestampFrequency = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;

    request.requestId = KmdSysman::Requests::Power::EnergyCounterUnits;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Power::CurrentEnergyCounter;
    vRequests.push_back(request);

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;
    request.requestId = KmdSysman::Requests::Activity::TimestampFrequency;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&energyUnits, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
    }

    uint32_t valueCounter = 0;
    uint64_t valueTimeStamp = 0;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&valueCounter, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        uint32_t conversionUnit = (1 << energyUnits);
        double valueConverted = static_cast<double>(valueCounter) / static_cast<double>(conversionUnit);
        valueConverted *= static_cast<double>(convertJouleToMicroJoule);
        pEnergy->energy = static_cast<uint64_t>(valueConverted);
        memcpy_s(&valueTimeStamp, sizeof(uint64_t), (vResponses[1].dataBuffer + sizeof(uint32_t)), sizeof(uint64_t));
    }

    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&timestampFrequency, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        double timeFactor = 1.0 / static_cast<double>(timestampFrequency);
        timeFactor = static_cast<double>(valueTimeStamp) * timeFactor;
        timeFactor *= static_cast<double>(microFacor);
        pEnergy->timestamp = static_cast<uint64_t>(timeFactor);
    }

    return ZE_RESULT_SUCCESS;
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

ze_result_t WddmPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmPowerImp::WddmPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmPowerImp *pWddmPowerImp = new WddmPowerImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsPower *>(pWddmPowerImp);
}

} // namespace L0

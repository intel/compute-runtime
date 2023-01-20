/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/windows/os_frequency_imp.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

namespace L0 {

ze_result_t WddmFrequencyImp::osFrequencyGetProperties(zes_freq_properties_t &properties) {
    readOverclockingInfo();
    uint32_t value = 0;
    properties.onSubdevice = false;
    properties.subdeviceId = 0;
    properties.type = frequencyDomainNumber;
    properties.isThrottleEventSupported = false;
    properties.min = unsupportedProperty;
    properties.max = unsupportedProperty;
    properties.max = unsupportedProperty;
    properties.canControl = false;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::FrequencyThrottledEventSupported;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::FrequencyRangeMinDefault;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::FrequencyRangeMaxDefault;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CanControlFrequency;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);
    vRequests.push_back(request);
    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        properties.isThrottleEventSupported = static_cast<ze_bool_t>(value);
    }

    if (vResponses[1].returnCode == ZE_RESULT_SUCCESS) {
        value = 0;
        memcpy_s(&value, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        properties.min = static_cast<double>(value);
    }

    if (vResponses[2].returnCode == ZE_RESULT_SUCCESS) {
        value = 0;
        memcpy_s(&value, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        properties.max = static_cast<double>(value);
    }

    if (vResponses[3].returnCode == ZE_RESULT_SUCCESS) {
        value = 0;
        memcpy_s(&value, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        properties.canControl = (value == 1);
    }

    return ZE_RESULT_SUCCESS;
}

double WddmFrequencyImp::osFrequencyGetStepSize() {
    return 50.0 / 3; // Step of 16.6666667 Mhz (GEN9 Hardcode);
}

ze_result_t WddmFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    return getRange(&pLimits->min, &pLimits->max);
}

ze_result_t WddmFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    return setRange(pLimits->min, pLimits->max);
}

ze_result_t WddmFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    uint32_t value = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentRequestedFrequency;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentTdpFrequency;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentResolvedFrequency;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentEfficientFrequency;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltage;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentThrottleReasons;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    pState->request = unsupportedProperty;
    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        pState->request = static_cast<double>(value);
    }

    pState->tdp = unsupportedProperty;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        pState->tdp = static_cast<double>(value);
    }

    pState->actual = unsupportedProperty;
    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        pState->actual = static_cast<double>(value);
    }

    pState->efficient = unsupportedProperty;
    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        pState->efficient = static_cast<double>(value);
    }

    pState->currentVoltage = unsupportedProperty;
    if (vResponses[4].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[4].dataBuffer, sizeof(uint32_t));
        pState->currentVoltage = static_cast<double>(value);
        pState->currentVoltage /= milliVoltsFactor;
    }

    if (vResponses[5].returnCode == KmdSysman::Success) {
        KmdThrottleReasons value = {0};
        pState->throttleReasons = {0};
        memcpy_s(&value, sizeof(uint32_t), vResponses[5].dataBuffer, sizeof(uint32_t));

        if (value.power3) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP;
        }
        if (value.power4) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP;
        }
        if (value.current1) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT;
        }
        if (value.thermal1 || value.thermal2 || value.thermal3 || value.thermal4) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFrequencyImp::getOcCapabilities(zes_oc_capabilities_t *pOcCapabilities) {
    *pOcCapabilities = ocCapabilities;
    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::getOcFrequencyTarget(double *pCurrentOcFrequency) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyTarget;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pCurrentOcFrequency = currentFrequencyTarget = static_cast<double>(value);

    return status;
}

ze_result_t WddmFrequencyImp::setOcFrequencyTarget(double currentOcFrequency) {
    this->currentFrequencyTarget = currentOcFrequency;
    return applyOcSettings();
}

ze_result_t WddmFrequencyImp::getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) {
    uint32_t unsignedValue = 0;
    int32_t signedValue = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&unsignedValue, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        *pCurrentVoltageTarget = currentVoltageTarget = static_cast<double>(unsignedValue);
    }

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&signedValue, sizeof(int32_t), vResponses[1].dataBuffer, sizeof(int32_t));
        *pCurrentVoltageOffset = currentVoltageOffset = static_cast<double>(signedValue);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) {
    this->currentVoltageTarget = currentVoltageTarget;
    this->currentVoltageOffset = currentVoltageOffset;
    return applyOcSettings();
}

ze_result_t WddmFrequencyImp::getOcMode(zes_oc_mode_t *pCurrentOcMode) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    currentVoltageMode = value ? ZES_OC_MODE_OVERRIDE : ZES_OC_MODE_INTERPOLATIVE;

    *pCurrentOcMode = currentVoltageMode;

    return status;
}

ze_result_t WddmFrequencyImp::setOcMode(zes_oc_mode_t currentOcMode) {
    if (currentOcMode == ZES_OC_MODE_FIXED) {
        this->currentVoltageMode = ZES_OC_MODE_INTERPOLATIVE;
        return ze_result_t::ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
    }

    if (currentOcMode == ZES_OC_MODE_OFF) {
        this->currentVoltageMode = ZES_OC_MODE_INTERPOLATIVE;
        return applyOcSettings();
    }

    if (currentOcMode == ZES_OC_MODE_INTERPOLATIVE || currentOcMode == ZES_OC_MODE_OVERRIDE) {
        this->currentVoltageMode = currentOcMode;
        return applyOcSettings();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::getOcIccMax(double *pOcIccMax) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentIccMax;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pOcIccMax = static_cast<double>(value);

    return status;
}

ze_result_t WddmFrequencyImp::setOcIccMax(double ocIccMax) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentIccMax;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    request.dataSize = sizeof(uint32_t);

    value = static_cast<uint32_t>(ocIccMax);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));

    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmFrequencyImp::getOcTjMax(double *pOcTjMax) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentTjMax;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pOcTjMax = static_cast<double>(value);

    return status;
}

ze_result_t WddmFrequencyImp::setOcTjMax(double ocTjMax) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentTjMax;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    request.dataSize = sizeof(uint32_t);

    value = static_cast<uint32_t>(ocTjMax);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));

    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmFrequencyImp::setRange(double min, double max) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyRange;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    request.dataSize = 2 * sizeof(uint32_t);

    value = static_cast<uint32_t>(min);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    value = static_cast<uint32_t>(max);
    memcpy_s((request.dataBuffer + sizeof(uint32_t)), sizeof(uint32_t), &value, sizeof(uint32_t));

    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmFrequencyImp::getRange(double *min, double *max) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyRange;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *min = static_cast<double>(value);
    memcpy_s(&value, sizeof(uint32_t), (response.dataBuffer + sizeof(uint32_t)), sizeof(uint32_t));
    *max = static_cast<double>(value);

    return status;
}

ze_result_t WddmFrequencyImp::applyOcSettings() {
    int32_t value = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    request.dataSize = sizeof(int32_t);

    // Fixed mode not supported.
    request.requestId = KmdSysman::Requests::Frequency::CurrentFixedMode;
    value = 0;
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;
    value = (currentVoltageMode == ZES_OC_MODE_OVERRIDE) ? 1 : 0;
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;
    value = static_cast<int32_t>(currentVoltageOffset);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;
    value = static_cast<int32_t>(currentVoltageTarget);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyTarget;
    value = static_cast<int32_t>(currentFrequencyTarget);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    for (uint32_t i = 0; i < vResponses.size(); i++) {
        if (vResponses[i].returnCode != KmdSysman::ReturnCodes::Success) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    return status;
}

void WddmFrequencyImp::readOverclockingInfo() {
    uint32_t value = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    request.requestId = KmdSysman::Requests::Frequency::ExtendedOcSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::FixedModeSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::HighVoltageModeSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::HighVoltageEnabled;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentIccMax;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::FrequencyOcSupported;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentTjMax;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::MaxNonOcFrequencyDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::MaxNonOcVoltageDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::MaxOcFrequencyDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::MaxOcVoltageDefault;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyTarget;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return;
    }

    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isExtendedModeSupported = static_cast<ze_bool_t>(value);
    }

    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[1].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isFixedModeSupported = static_cast<ze_bool_t>(value);
    }

    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isHighVoltModeCapable = static_cast<ze_bool_t>(value);
    }

    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isHighVoltModeEnabled = static_cast<ze_bool_t>(value);
    }

    if (vResponses[4].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[4].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isIccMaxSupported = static_cast<ze_bool_t>(value > 0);
    }

    if (vResponses[5].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[5].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isOcSupported = static_cast<ze_bool_t>(value);
    }

    if (vResponses[6].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[6].dataBuffer, sizeof(uint32_t));
        ocCapabilities.isTjMaxSupported = static_cast<ze_bool_t>(value > 0);
    }

    if (vResponses[7].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[7].dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxFactoryDefaultFrequency = static_cast<double>(value);
    }

    if (vResponses[8].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[8].dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxFactoryDefaultVoltage = static_cast<double>(value);
    }

    if (vResponses[9].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[9].dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxOcFrequency = static_cast<double>(value);
    }

    if (vResponses[10].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[10].dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxOcVoltage = static_cast<double>(value);
    }

    if (vResponses[11].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[11].dataBuffer, sizeof(uint32_t));
        currentFrequencyTarget = static_cast<double>(value);
    }

    if (vResponses[12].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[12].dataBuffer, sizeof(uint32_t));
        currentVoltageTarget = static_cast<double>(value);
    }

    if (vResponses[13].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[13].dataBuffer, sizeof(uint32_t));
        currentVoltageOffset = static_cast<double>(value);
    }

    if (vResponses[14].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[14].dataBuffer, sizeof(uint32_t));
        currentVoltageMode = value ? ZES_OC_MODE_OVERRIDE : ZES_OC_MODE_INTERPOLATIVE;
    }
}

WddmFrequencyImp::WddmFrequencyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    this->frequencyDomainNumber = frequencyDomainNumber;
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsFrequency *OsFrequency::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t frequencyDomainNumber) {
    WddmFrequencyImp *pWddmFrequencyImp = new WddmFrequencyImp(pOsSysman, onSubdevice, subdeviceId, frequencyDomainNumber);
    return static_cast<OsFrequency *>(pWddmFrequencyImp);
}

uint16_t OsFrequency::getNumberOfFreqDoainsSupported(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::NumFrequencyDomains;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return 0;
    }

    uint32_t maxNumEnginesSupported = 0;
    memcpy_s(&maxNumEnginesSupported, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    return static_cast<uint16_t>(maxNumEnginesSupported);
}

} // namespace L0

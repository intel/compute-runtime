/*
 * Copyright (C) 2019-2020 Intel Corporation
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
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::FrequencyThrottledEventSupported;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);

    properties.isThrottleEventSupported = false;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        properties.isThrottleEventSupported = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::FrequencyRangeMinDefault;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);

    properties.min = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        value = 0;
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        properties.min = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::FrequencyRangeMaxDefault;
    request.paramInfo = static_cast<uint32_t>(frequencyDomainNumber);

    properties.max = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        value = 0;
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        properties.max = static_cast<double>(value);
    }

    properties.onSubdevice = false;
    properties.subdeviceId = 0;
    properties.type = frequencyDomainNumber;
    properties.canControl = canControl();

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::osFrequencyGetRange(zes_freq_range_t *pLimits) {
    return getRange(&pLimits->min, &pLimits->max);
}

ze_result_t WddmFrequencyImp::osFrequencySetRange(const zes_freq_range_t *pLimits) {
    return setRange(pLimits->min, pLimits->max);
}

ze_result_t WddmFrequencyImp::osFrequencyGetState(zes_freq_state_t *pState) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentRequestedFrequency;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    pState->request = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        pState->request = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentTdpFrequency;

    pState->tdp = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        pState->tdp = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentResolvedFrequency;

    pState->actual = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        pState->actual = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentEfficientFrequency;

    pState->efficient = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        pState->efficient = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltage;

    pState->currentVoltage = unsupportedProperty;
    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        pState->currentVoltage = static_cast<double>(value);
        pState->currentVoltage /= milliVoltsFactor;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentThrottleReasons;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        KmdThrottleReasons value = {0};
        pState->throttleReasons = {0};
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

        if (value.powerlimit1) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP;
        }
        if (value.powerlimit2) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP;
        }
        if (value.powerlimit4) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT;
        }
        if (value.thermal) {
            pState->throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::osFrequencyGetThrottleTime(zes_freq_throttle_time_t *pThrottleTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool WddmFrequencyImp::canControl() {
    double minF = 0.0, maxF = 0.0;
    if (getRange(&minF, &maxF) != ZE_RESULT_SUCCESS) {
        return false;
    }

    return (setRange(minF, maxF) == ZE_RESULT_SUCCESS);
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
    if (currentFrequencyTarget != currentOcFrequency) {
        currentFrequencyTarget = currentOcFrequency;
        return applyOcSettings();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::getOcVoltageTarget(double *pCurrentVoltageTarget, double *pCurrentVoltageOffset) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pCurrentVoltageTarget = currentVoltageTarget = static_cast<double>(value);

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    *pCurrentVoltageOffset = currentVoltageOffset = static_cast<double>(value);

    return status;
}

ze_result_t WddmFrequencyImp::setOcVoltageTarget(double currentVoltageTarget, double currentVoltageOffset) {
    if (this->currentVoltageTarget != currentVoltageTarget || this->currentVoltageOffset != currentVoltageOffset) {
        this->currentVoltageTarget = currentVoltageTarget;
        this->currentVoltageOffset = currentVoltageOffset;
        return applyOcSettings();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFrequencyImp::getOcMode(zes_oc_mode_t *pCurrentOcMode) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentFixedMode;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    currentFixedMode = value ? ZES_OC_MODE_FIXED : ZES_OC_MODE_OFF;

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    currentVoltageMode = value ? ZES_OC_MODE_OVERRIDE : ZES_OC_MODE_INTERPOLATIVE;

    if (currentFixedMode == ZES_OC_MODE_FIXED) {
        currentOcMode = ZES_OC_MODE_FIXED;
    } else {
        currentOcMode = currentVoltageMode;
    }

    *pCurrentOcMode = currentOcMode;

    return status;
}

ze_result_t WddmFrequencyImp::setOcMode(zes_oc_mode_t currentOcMode) {
    if (currentOcMode == ZES_OC_MODE_OFF) {
        this->currentFrequencyTarget = ocCapabilities.maxFactoryDefaultFrequency;
        this->currentVoltageTarget = ocCapabilities.maxFactoryDefaultVoltage;
        this->currentVoltageOffset = 0;
        this->currentFixedMode = ZES_OC_MODE_OFF;
        this->currentVoltageMode = ZES_OC_MODE_INTERPOLATIVE;
        this->currentOcMode = ZES_OC_MODE_OFF;
        return applyOcSettings();
    }

    if (currentOcMode == ZES_OC_MODE_FIXED) {
        this->currentOcMode = ZES_OC_MODE_FIXED;
        this->currentFixedMode = ZES_OC_MODE_FIXED;
        this->currentVoltageMode = ZES_OC_MODE_OVERRIDE;
        return applyOcSettings();
    }

    if (currentOcMode == ZES_OC_MODE_INTERPOLATIVE || currentOcMode == ZES_OC_MODE_OVERRIDE) {
        this->currentVoltageMode = currentOcMode;
        this->currentFixedMode = ZES_OC_MODE_OFF;
        this->currentOcMode = currentOcMode;
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
    request.requestId = KmdSysman::Requests::Frequency::CurrentIccMax;
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
    ze_result_t status = ZE_RESULT_SUCCESS;
    int32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::CurrentFixedMode;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);
    request.dataSize = sizeof(int32_t);

    value = (currentFixedMode == ZES_OC_MODE_FIXED) ? 1 : 0;
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;

    value = (currentVoltageMode == ZES_OC_MODE_OVERRIDE) ? 1 : 0;
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;

    value = static_cast<int32_t>(currentVoltageOffset);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;

    value = static_cast<int32_t>(currentVoltageTarget);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyTarget;

    value = static_cast<int32_t>(currentFrequencyTarget);
    memcpy_s(request.dataBuffer, sizeof(int32_t), &value, sizeof(int32_t));

    return pKmdSysManager->requestSingle(request, response);
}

void WddmFrequencyImp::readOverclockingInfo() {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FrequencyComponent;
    request.requestId = KmdSysman::Requests::Frequency::ExtendedOcSupported;
    request.paramInfo = static_cast<uint32_t>(this->frequencyDomainNumber);

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isExtendedModeSupported = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::FixedModeSupported;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isFixedModeSupported = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::HighVoltageModeSupported;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isHighVoltModeCapable = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::HighVoltageEnabled;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isHighVoltModeEnabled = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentIccMax;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isIccMaxSupported = static_cast<ze_bool_t>(value > 0);
    }

    request.requestId = KmdSysman::Requests::Frequency::FrequencyOcSupported;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isOcSupported = static_cast<ze_bool_t>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentTjMax;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.isTjMaxSupported = static_cast<ze_bool_t>(value > 0);
    }

    request.requestId = KmdSysman::Requests::Frequency::MaxNonOcFrequencyDefault;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxFactoryDefaultFrequency = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::MaxNonOcVoltageDefault;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxFactoryDefaultVoltage = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::MaxOcFrequencyDefault;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxOcFrequency = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::MaxOcVoltageDefault;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        ocCapabilities.maxOcVoltage = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentFixedMode;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        currentFixedMode = value ? ZES_OC_MODE_FIXED : ZES_OC_MODE_OFF;
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentFrequencyTarget;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        currentFrequencyTarget = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageTarget;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        currentVoltageTarget = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageOffset;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        currentVoltageOffset = static_cast<double>(value);
    }

    request.requestId = KmdSysman::Requests::Frequency::CurrentVoltageMode;

    if (pKmdSysManager->requestSingle(request, response) == ZE_RESULT_SUCCESS) {
        memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
        currentVoltageMode = value ? ZES_OC_MODE_OVERRIDE : ZES_OC_MODE_INTERPOLATIVE;
    }

    if (currentFrequencyTarget == 0.0 || currentVoltageTarget == 0.0) {
        if (currentFrequencyTarget == 0.0) {
            currentFrequencyTarget = ocCapabilities.maxFactoryDefaultFrequency;
        }

        if (currentVoltageTarget == 0.0) {
            currentVoltageTarget = ocCapabilities.maxFactoryDefaultVoltage;
        }
    }

    if (currentFixedMode == ZES_OC_MODE_FIXED) {
        currentOcMode = ZES_OC_MODE_FIXED;
    } else {
        currentOcMode = currentVoltageMode;
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

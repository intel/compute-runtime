/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/windows/os_power_imp.h"

#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"

#include <algorithm>

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
    uint64_t energyCounter64Bit = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.requestId = KmdSysman::Requests::Power::CurrentEnergyCounter64Bit;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        memcpy_s(&energyCounter64Bit, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
        pEnergy->energy = energyCounter64Bit;
        pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();
    }

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

    ze_bool_t enabled = false;
    memcpy_s(&enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));
    powerLimitCount = 0;

    if ((status == ZE_RESULT_SUCCESS) && enabled) {
        powerLimitCount++;
    }

    request.requestId = KmdSysman::Requests::Power::PowerLimit2Enabled;
    status = pKmdSysManager->requestSingle(request, response);
    enabled = false;
    memcpy_s(&enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

    if ((status == ZE_RESULT_SUCCESS) && enabled) {
        powerLimitCount++;
    }

    request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Ac;
    status = pKmdSysManager->requestSingle(request, response);

    if (status == ZE_RESULT_SUCCESS) {
        powerLimitCount++;
    }

    if (powerLimitCount > 0) {
        return true;
    } else {
        return false;
    }
}

ze_result_t WddmPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint8_t count = 0;

    if (*pCount == 0) {
        *pCount = powerLimitCount;
        return result;
    }

    *pCount = std::min(*pCount, powerLimitCount);

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::PowerComponent;

    if (pSustained) {
        // sustained
        request.requestId = KmdSysman::Requests::Power::PowerLimit1Enabled;
        result = pKmdSysManager->requestSingle(request, response);

        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        ze_bool_t enabled = false;
        memcpy_s(&enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

        if (enabled) {
            memset(&pSustained[count], 0, sizeof(zes_power_limit_ext_desc_t));
            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;

            result = pKmdSysManager->requestSingle(request, response);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }

            uint32_t limit = 0;
            memcpy_s(&limit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1Tau;

            result = pKmdSysManager->requestSingle(request, response);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }

            uint32_t interval = 0;
            memcpy_s(&interval, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
            pSustained[count].enabled = enabled;
            pSustained[count].limit = limit;
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = false;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_SUSTAINED;
            pSustained[count].limitUnit = ZES_LIMIT_UNIT_POWER;
            pSustained[count].interval = interval;
            count++;
        }

        // Burst
        request.requestId = KmdSysman::Requests::Power::PowerLimit2Enabled;
        result = pKmdSysManager->requestSingle(request, response);

        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        enabled = false;
        memcpy_s(&enabled, sizeof(ze_bool_t), response.dataBuffer, sizeof(ze_bool_t));

        if (count < *pCount && enabled) {
            memset(&pSustained[count], 0, sizeof(zes_power_limit_ext_desc_t));
            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit2;
            result = pKmdSysManager->requestSingle(request, response);

            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }

            uint32_t limit = 0;
            memcpy_s(&limit, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

            pSustained[count].enabled = enabled;
            pSustained[count].limit = limit;
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = false;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_BURST;
            pSustained[count].limitUnit = ZES_LIMIT_UNIT_POWER;
            pSustained[count].interval = 0;
            count++;
        }
        // Peak
        if (count < *pCount) {

            memset(&pSustained[count], 0, sizeof(zes_power_limit_ext_desc_t));
            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Ac;
            result = pKmdSysManager->requestSingle(request, response);

            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }

            uint32_t powerAC = 0;
            memcpy_s(&powerAC, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Dc;

            result = pKmdSysManager->requestSingle(request, response);

            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            uint32_t powerDC = 0;
            memcpy_s(&powerDC, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

            pSustained[count].enabled = true;
            pSustained[count].limit = powerAC;
            pSustained[count].enabledStateLocked = true;
            pSustained[count].intervalValueLocked = false;
            pSustained[count].limitValueLocked = false;
            pSustained[count].source = ZES_POWER_SOURCE_ANY;
            pSustained[count].level = ZES_POWER_LEVEL_PEAK;
            pSustained[count].limitUnit = ZES_LIMIT_UNIT_POWER;
            pSustained[count].interval = 0;
            count++;
        }
    }
    return result;
}

ze_result_t WddmPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::PowerComponent;
    request.dataSize = sizeof(uint32_t);

    for (uint32_t i = 0; i < *pCount; i++) {
        if (pSustained[i].level == ZES_POWER_LEVEL_SUSTAINED) {
            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1;
            memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained[i].limit, sizeof(uint32_t));
            status = pKmdSysManager->requestSingle(request, response);
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }

            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit1Tau;
            memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained[i].interval, sizeof(uint32_t));
            status = pKmdSysManager->requestSingle(request, response);
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }
        } else if (pSustained[i].level == ZES_POWER_LEVEL_BURST) {
            request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit2;
            memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained[i].limit, sizeof(uint32_t));
            status = pKmdSysManager->requestSingle(request, response);
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }
        } else if (pSustained[i].level == ZES_POWER_LEVEL_PEAK) {
            if (pSustained[i].source != ZES_POWER_SOURCE_BATTERY) {
                request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Ac;
                memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained[i].limit, sizeof(uint32_t));
                status = pKmdSysManager->requestSingle(request, response);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }
            } else {
                request.requestId = KmdSysman::Requests::Power::CurrentPowerLimit4Dc;
                memcpy_s(request.dataBuffer, sizeof(uint32_t), &pSustained[i].limit, sizeof(uint32_t));
                status = pKmdSysManager->requestSingle(request, response);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }
            }
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }
    return status;
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

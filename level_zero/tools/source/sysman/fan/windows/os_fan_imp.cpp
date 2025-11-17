/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/windows/os_fan_imp.h"

#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"

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

union FanCapability {
    struct
    {
        int32_t allSupported : 1;    /// Allows a single usermode fan table to be applied to all fan devices.
        int32_t singleSupported : 1; /// Allows different usermode fan tables to be configured.
        int32_t reserved : 30;       /// Reserved Bits
    };
    int32_t data;
};

ze_result_t WddmFanImp::getProperties(zes_fan_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    // Set fan index only if multiple fans are supported
    setFanIndexForMultipleFans(vRequests);

    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    request.dataSize = sizeof(uint32_t);

    uint32_t fanPoints = 2;
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &fanPoints, sizeof(uint32_t));

    vRequests.push_back(request);

    request.dataSize = 0;
    memset(request.dataBuffer, request.dataSize, sizeof(request.dataBuffer));
    request.commandId = KmdSysman::Command::Get;
    request.requestId = KmdSysman::Requests::Fans::MaxFanControlPointsSupported;

    vRequests.push_back(request);

    // Add request to get max fan speed (RPM)
    request.requestId = KmdSysman::Requests::Fans::MaxFanSpeedSupported;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (vResponses.size() != vRequests.size()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Calculate response offset based on whether fan index requests were added
    uint32_t responseOffset = getResponseOffset();

    pProperties->canControl = (vResponses[responseOffset].returnCode == KmdSysman::Success);

    if (vResponses[responseOffset + 1].returnCode == KmdSysman::Success) {
        memcpy_s(&fanPoints, sizeof(uint32_t), vResponses[responseOffset + 1].dataBuffer, sizeof(uint32_t));
        pProperties->maxPoints = maxPoints = static_cast<int32_t>(fanPoints);
    }

    // Get max fan speed (RPM)
    pProperties->maxRPM = -1; // Default to unsupported
    if (vResponses[responseOffset + 2].returnCode == KmdSysman::Success) {
        uint32_t maxRPS = 0;
        memcpy_s(&maxRPS, sizeof(uint32_t), vResponses[responseOffset + 2].dataBuffer, sizeof(uint32_t));
        pProperties->maxRPM = static_cast<int32_t>(maxRPS) * 60;
    }
    pProperties->supportedModes = zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE;
    pProperties->supportedUnits = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFanImp::getConfig(zes_fan_config_t *pConfig) {
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};

    setFanIndexForMultipleFans(vRequests);

    KmdSysman::RequestProperty request = {};
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if (vResponses.size() != vRequests.size()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint32_t responseOffset = getResponseOffset();
    uint32_t value = 0;
    if (vResponses[responseOffset].returnCode == KmdSysman::Success) {
        memcpy_s(&value, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
    }

    if (value == 0) {
        return handleDefaultMode(pConfig);
    } else {
        return handleTableMode(pConfig, value);
    }
}

ze_result_t WddmFanImp::handleDefaultMode(zes_fan_config_t *pConfig) {
    // Default mode - need to get max fan points
    pConfig->mode = ZES_FAN_SPEED_MODE_DEFAULT;
    pConfig->speedTable.numPoints = 0;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    setFanIndexForMultipleFans(vRequests);

    KmdSysman::RequestProperty request = {};
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::MaxFanControlPointsSupported;
    vRequests.push_back(request);

    uint32_t responseOffset = getResponseOffset();
    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);
    if ((status == ZE_RESULT_SUCCESS) && (vResponses.size() == vRequests.size()) && (vResponses[responseOffset].returnCode == KmdSysman::Success)) {
        uint32_t maxFanPoints = 0;
        memcpy_s(&maxFanPoints, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
        pConfig->speedTable.numPoints = static_cast<int32_t>(maxFanPoints);

        // Try reading Default Fan table if the platform supports
        if (maxFanPoints != 0) {
            for (int32_t i = 0; i < pConfig->speedTable.numPoints; i++) {
                vRequests.clear();
                vResponses.clear();
                setFanIndexForMultipleFans(vRequests);

                request = {};
                request.commandId = KmdSysman::Command::Get;
                request.componentId = KmdSysman::Component::FanComponent;
                request.requestId = KmdSysman::Requests::Fans::CurrentFanPoint;
                vRequests.push_back(request);

                status = pKmdSysManager->requestMultiple(vRequests, vResponses);
                if ((status == ZE_RESULT_SUCCESS) && (vResponses.size() == vRequests.size()) && (vResponses[responseOffset].returnCode == KmdSysman::Success)) {
                    FanPoint point = {};
                    memcpy_s(&point.data, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
                    pConfig->speedTable.table[i].speed.speed = point.fanSpeedPercent;
                    pConfig->speedTable.table[i].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
                    pConfig->speedTable.table[i].temperature = point.temperatureDegreesCelsius;
                } else {
                    pConfig->speedTable.numPoints = i;
                    break;
                }
            }
        }
    } // else, return Success. We still return valid FanConfig.mode

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFanImp::handleTableMode(zes_fan_config_t *pConfig, uint32_t numPoints) {
    // Table mode - need to read existing fan points
    pConfig->mode = ZES_FAN_SPEED_MODE_TABLE;
    pConfig->speedTable.numPoints = numPoints;

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    uint32_t responseOffset = getResponseOffset();

    for (int32_t i = 0; i < pConfig->speedTable.numPoints; i++) {
        vRequests.clear();
        vResponses.clear();
        setFanIndexForMultipleFans(vRequests);

        KmdSysman::RequestProperty request = {};
        request.commandId = KmdSysman::Command::Get;
        request.componentId = KmdSysman::Component::FanComponent;
        request.requestId = KmdSysman::Requests::Fans::CurrentFanPoint;
        vRequests.push_back(request);

        ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);
        if ((status == ZE_RESULT_SUCCESS) && (vResponses.size() == vRequests.size()) && (vResponses[responseOffset].returnCode == KmdSysman::Success)) {
            FanPoint point = {};
            memcpy_s(&point.data, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
            pConfig->speedTable.table[i].speed.speed = point.fanSpeedPercent;
            pConfig->speedTable.table[i].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
            pConfig->speedTable.table[i].temperature = point.temperatureDegreesCelsius;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmFanImp::setDefaultMode() {
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};

    // Set fan index only if multiple fans are supported
    setFanIndexForMultipleFans(vRequests);

    KmdSysman::RequestProperty request = {};

    // Passing current number of control points as zero will reset pcode to default fan curve
    uint32_t value = 0; // 0 to reset to default
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentNumOfControlPoints;
    request.dataSize = sizeof(uint32_t);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    vRequests.push_back(request);

    return pKmdSysManager->requestMultiple(vRequests, vResponses);
}

ze_result_t WddmFanImp::setFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFanImp::setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};

    // Set fan index first only if multiple fans are supported
    setFanIndexForMultipleFans(vRequests);

    KmdSysman::RequestProperty request = {};
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::MaxFanControlPointsSupported;
    vRequests.push_back(request);

    uint32_t fanPoints = 2;

    uint32_t responseOffset = getResponseOffset();
    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if ((vResponses.size() == vRequests.size()) && (vResponses[responseOffset].returnCode == KmdSysman::Success)) {
        memcpy_s(&fanPoints, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
        maxPoints = static_cast<int32_t>(fanPoints);
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (pSpeedTable->numPoints == 0 || pSpeedTable->numPoints > maxPoints) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (int32_t i = 0; i < pSpeedTable->numPoints; i++) {
        if (pSpeedTable->table[i].speed.units == zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    // Clear the vectors and prepare for the actual speed table setting
    vRequests.clear();
    vResponses.clear();

    // Set fan index first only if multiple fans are supported
    setFanIndexForMultipleFans(vRequests);

    uint32_t value = pSpeedTable->numPoints;

    request = {};
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
}

ze_result_t WddmFanImp::getState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    if (units == ZES_FAN_SPEED_UNITS_PERCENT) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    setFanIndexForMultipleFans(vRequests);

    KmdSysman::RequestProperty request = {};
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanSpeed;
    vRequests.push_back(request);

    uint32_t responseOffset = getResponseOffset();
    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    if ((vResponses.size() != vRequests.size()) || (vResponses[responseOffset].returnCode != KmdSysman::Success)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint32_t value = 0;
    memcpy_s(&value, sizeof(uint32_t), vResponses[responseOffset].dataBuffer, sizeof(uint32_t));
    *pSpeed = static_cast<int32_t>(value);

    return ZE_RESULT_SUCCESS;
}

WddmFanImp::WddmFanImp(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported)
    : fanIndex(fanIndex), multipleFansSupported(multipleFansSupported) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

std::unique_ptr<OsFan> OsFan::create(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported) {
    std::unique_ptr<WddmFanImp> pWddmFanImp = std::make_unique<WddmFanImp>(pOsSysman, fanIndex, multipleFansSupported);
    return pWddmFanImp;
}

uint32_t OsFan::getSupportedFanCount(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    // Get the number of fan domains
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::NumFanDomains;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return 0;
    }

    uint32_t fanCount = 0;
    memcpy_s(&fanCount, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    // If fanCount > 1, check if SingleSupported mode is available
    if (fanCount > 1) {
        request.commandId = KmdSysman::Command::Get;
        request.componentId = KmdSysman::Component::FanComponent;
        request.requestId = KmdSysman::Requests::Fans::SupportedFanModeCapabilities;

        status = pKmdSysManager->requestSingle(request, response);

        if (status == ZE_RESULT_SUCCESS) {
            FanCapability fanCaps = {};
            memcpy_s(&fanCaps.data, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

            // Check SingleSupported capability
            bool singleSupported = (fanCaps.singleSupported != 0);

            // If SingleSupported is not available, return fanCount as 1
            if (!singleSupported) {
                fanCount = 1;
            }
        }
    }

    return fanCount;
}

void WddmFanImp::setFanIndexForMultipleFans(std::vector<KmdSysman::RequestProperty> &vRequests) {
    if (!multipleFansSupported) {
        return;
    }

    KmdSysman::RequestProperty request = {};

    // First, set fan mode to single
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanMode;
    request.dataSize = sizeof(uint32_t);

    uint32_t fanMode = KmdSysman::FanUserMode::FanModeSingle;
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &fanMode, sizeof(uint32_t));
    vRequests.push_back(request);

    // Then, set the fan index
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::FanComponent;
    request.requestId = KmdSysman::Requests::Fans::CurrentFanIndex;
    request.dataSize = sizeof(uint32_t);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &fanIndex, sizeof(uint32_t));
    vRequests.push_back(request);
}

uint32_t WddmFanImp::getResponseOffset() const {
    return multipleFansSupported ? 2 : 0;
}

} // namespace L0

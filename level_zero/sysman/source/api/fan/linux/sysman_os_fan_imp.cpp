/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/linux/sysman_os_fan_imp.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>
#include <regex>
#include <vector>

namespace L0 {
namespace Sysman {

static constexpr int32_t pwmEnableFullSpeed = 0;
static constexpr int32_t pwmEnableManualUserTable = 1;
static constexpr int32_t pwmEnableAutoStockTable = 2;

static constexpr int32_t pwmMax = 255;
static constexpr int32_t milliDegreesPerDegree = 1000;

ze_result_t LinuxFanImp::getProperties(zes_fan_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    pProperties->canControl = pSysfsAccess->fileExists(pwmNode);

    pProperties->maxRPM = -1;
    int32_t maxRpm = 0;
    if (pSysfsAccess->read(fanMaxNode, maxRpm) == ZE_RESULT_SUCCESS) {
        pProperties->maxRPM = maxRpm;
    }

    pProperties->maxPoints = 0;
    for (uint32_t pointIndex = 0; pointIndex < maxFanControlPoints; pointIndex++) {
        if (!pSysfsAccess->fileExists(autoPointTempNodes[pointIndex])) {
            break;
        }
        pProperties->maxPoints = static_cast<int32_t>(pointIndex + 1);
    }

    pProperties->supportedModes = (1u << ZES_FAN_SPEED_MODE_DEFAULT) | (1u << ZES_FAN_SPEED_MODE_TABLE);
    pProperties->supportedUnits = (1u << ZES_FAN_SPEED_UNITS_RPM);

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::readCurvePoints(zes_fan_config_t *pConfig, uint32_t numPoints) {
    for (uint32_t pointIndex = 0; pointIndex < numPoints; pointIndex++) {
        int32_t tempMilliDeg = 0;
        if (pSysfsAccess->read(autoPointTempNodes[pointIndex], tempMilliDeg) != ZE_RESULT_SUCCESS) {
            pConfig->speedTable.numPoints = static_cast<int32_t>(pointIndex);
            break;
        }

        int32_t pwmVal = 0;
        if (pSysfsAccess->read(autoPointPwmNodes[pointIndex], pwmVal) != ZE_RESULT_SUCCESS) {
            pConfig->speedTable.numPoints = static_cast<int32_t>(pointIndex);
            break;
        }

        pConfig->speedTable.table[pointIndex].temperature = tempMilliDeg / milliDegreesPerDegree;
        pConfig->speedTable.table[pointIndex].speed.speed = (pwmVal * 100) / pwmMax;
        pConfig->speedTable.table[pointIndex].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::getConfig(zes_fan_config_t *pConfig) {
    // Fixed speed mode is not supported; always report an invalid sentinel.
    pConfig->speedFixed = {-1, ZES_FAN_SPEED_UNITS_RPM};

    int32_t enableMode = pwmEnableAutoStockTable;
    if (pSysfsAccess->read(pwmEnableNode, enableMode) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    if (enableMode == pwmEnableAutoStockTable || enableMode == pwmEnableFullSpeed) {
        pConfig->mode = ZES_FAN_SPEED_MODE_DEFAULT;
        pConfig->speedTable.numPoints = 0;
        return ZE_RESULT_SUCCESS;
    }

    pConfig->mode = ZES_FAN_SPEED_MODE_TABLE;
    pConfig->speedTable.numPoints = static_cast<int32_t>(maxFanControlPoints);

    for (uint32_t pointIndex = 0; pointIndex < maxFanControlPoints; pointIndex++) {
        if (!pSysfsAccess->fileExists(autoPointTempNodes[pointIndex])) {
            pConfig->speedTable.numPoints = static_cast<int32_t>(pointIndex);
            break;
        }
    }

    return readCurvePoints(pConfig, static_cast<uint32_t>(pConfig->speedTable.numPoints));
}

ze_result_t LinuxFanImp::getState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    if (units == ZES_FAN_SPEED_UNITS_PERCENT) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    int32_t rpm = 0;
    ze_result_t result = pSysfsAccess->read(fanInputNode, rpm);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *pSpeed = rpm;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::setDefaultMode() {
    return pSysfsAccess->write(pwmEnableNode, pwmEnableAutoStockTable);
}

ze_result_t LinuxFanImp::setFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFanImp::setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    // The table must have at least one point and no more than the KMD-supported maximum
    // (pwm1_auto_point1_* .. pwm1_auto_point10_*).
    if (pSpeedTable->numPoints <= 0 ||
        pSpeedTable->numPoints > static_cast<int32_t>(maxFanControlPoints)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Validate units and speed value for every entry before touching hardware.
    // Only percent units are accepted because the KMD stores raw PWM duty (0-255)
    // and there is no valid RPM-to-PWM conversion available at this layer.
    // Speed must be in [0, 100] to be a valid percentage.
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        if (pSpeedTable->table[pointIndex].speed.units == ZES_FAN_SPEED_UNITS_RPM) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (pSpeedTable->table[pointIndex].speed.speed < 0 ||
            pSpeedTable->table[pointIndex].speed.speed > 100) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    // Pre-flight check: verify that every required pwm1_auto_point[N]_temp sysfs node
    // exists before writing anything. This prevents a partial update where some points
    // are written and others are not because the KMD exposes fewer nodes than requested.
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        if (!pSysfsAccess->fileExists(autoPointTempNodes[static_cast<uint32_t>(pointIndex)])) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    // Switch to manual table mode (pwm1_enable = 1) BEFORE writing auto_point values.
    // The KMD rejects writes to pwm1_auto_point*_temp/pwm with EINVAL while the fan
    // is in auto/stock mode (pwm1_enable = 2); manual mode must be active first.
    ze_result_t result = pSysfsAccess->write(pwmEnableNode, pwmEnableManualUserTable);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // Write each temperature/PWM pair to the corresponding sysfs nodes.
    // Temperature: ZES uses degrees Celsius; the kernel expects millidegrees,
    //              so multiply by milliDegreesPerDegree (1000). e.g. 60C -> 60000.
    // PWM duty:    ZES uses 0-100%; the kernel uses 0-255 raw PWM,
    //              so scale with (speed% * pwmMax) / 100. e.g. 50% -> 127.
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        const int32_t tempMilliDeg = pSpeedTable->table[pointIndex].temperature * milliDegreesPerDegree;
        result = pSysfsAccess->write(autoPointTempNodes[static_cast<uint32_t>(pointIndex)], tempMilliDeg);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }

        const int32_t pwmVal = (pSpeedTable->table[pointIndex].speed.speed * pwmMax) / 100;
        result = pSysfsAccess->write(autoPointPwmNodes[static_cast<uint32_t>(pointIndex)], pwmVal);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    return ZE_RESULT_SUCCESS;
}

static std::string findHwmonDir(SysFsAccessInterface *pSysfsAccess, SysmanKmdInterface *pSysmanKmdInterface,
                                uint32_t subdeviceId, ze_bool_t isSubdevice) {
    const std::string hwmonBaseDir("device/hwmon");
    std::vector<std::string> hwmonEntries;
    if (pSysfsAccess->scanDirEntries(hwmonBaseDir, hwmonEntries) != ZE_RESULT_SUCCESS) {
        return {};
    }
    for (const auto &entry : hwmonEntries) {
        const std::string nameFile = hwmonBaseDir + "/" + entry + "/name";
        std::string name;
        if (pSysfsAccess->read(nameFile, name) != ZE_RESULT_SUCCESS) {
            continue;
        }
        if (name == pSysmanKmdInterface->getHwmonName(subdeviceId, isSubdevice)) {
            return hwmonBaseDir + "/" + entry;
        }
    }
    return {};
}

void LinuxFanImp::init() {
    hwmonDir = findHwmonDir(pSysfsAccess, pSysmanKmdInterface, 0, false);
    if (hwmonDir.empty()) {
        return;
    }
    const uint32_t ch = channel();
    fanInputNode = pSysmanKmdInterface->getFanInputNode(hwmonDir, ch);
    fanMaxNode = pSysmanKmdInterface->getFanMaxNode(hwmonDir, ch);
    pwmNode = pSysmanKmdInterface->getPwmNode(hwmonDir, ch);
    pwmEnableNode = pSysmanKmdInterface->getPwmEnableNode(hwmonDir, ch);
    for (uint32_t pointIndex = 0; pointIndex < maxFanControlPoints; pointIndex++) {
        autoPointTempNodes[pointIndex] = pSysmanKmdInterface->getPwmAutoPointTempNode(hwmonDir, ch, pointIndex);
        autoPointPwmNodes[pointIndex] = pSysmanKmdInterface->getPwmAutoPointPwmNode(hwmonDir, ch, pointIndex);
    }
}

LinuxFanImp::LinuxFanImp(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported)
    : fanIndex(fanIndex) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
    init();
}

std::unique_ptr<OsFan> OsFan::create(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported) {
    std::unique_ptr<LinuxFanImp> pLinuxFanImp = std::make_unique<LinuxFanImp>(pOsSysman, fanIndex, multipleFansSupported);
    return pLinuxFanImp;
}

std::vector<uint32_t> OsFan::getSupportedFanChannels(OsSysman *pOsSysman) {
    auto *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto *pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto *pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();

    const std::string hwmonDir = findHwmonDir(pSysfsAccess, pSysmanKmdInterface, 0, false);

    if (hwmonDir.empty()) {
        return {};
    }

    // Enumerate fans by scanning the hwmon directory for fan[N]_input entries and
    // recording the actual channel numbers.  This correctly handles platforms that
    // expose non-contiguous channels (e.g. fan1_input + fan3_input).
    std::vector<std::string> hwmonFiles;
    if (pSysfsAccess->scanDirEntries(hwmonDir, hwmonFiles) != ZE_RESULT_SUCCESS) {
        return {};
    }

    // Match entries of the form "fan<digits>_input" and capture the channel number.
    const std::regex fanInputPattern("^fan([0-9]+)_input$");

    std::vector<uint32_t> channels;
    for (const auto &file : hwmonFiles) {
        std::smatch match;
        if (std::regex_match(file, match, fanInputPattern)) {
            channels.push_back(static_cast<uint32_t>(std::stoul(match[1].str())));
        }
    }
    std::sort(channels.begin(), channels.end());
    return channels;
}

} // namespace Sysman
} // namespace L0

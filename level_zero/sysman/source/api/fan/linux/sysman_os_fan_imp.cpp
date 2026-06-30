/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/linux/sysman_os_fan_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>
#include <regex>
#include <vector>

namespace L0 {
namespace Sysman {

static constexpr int32_t pwmEnableManualUserTable = 1;
static constexpr int32_t pwmEnableAutoStockTable = 2;

static constexpr int32_t pwmMax = 255;
static constexpr int32_t milliDegreesPerDegree = 1000;

ze_result_t LinuxFanImp::getProperties(zes_fan_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;

    pProperties->canControl = pSysfsAccess->fileExists(pwmNode);

    pProperties->maxRPM = maxRpm;

    pProperties->maxPoints = 0;
    for (uint32_t pointIndex = 0; pointIndex < maxFanControlPoints; pointIndex++) {
        if (!pSysfsAccess->fileExists(autoPointTempNodes[pointIndex])) {
            break;
        }
        pProperties->maxPoints = static_cast<int32_t>(pointIndex + 1);
    }

    pProperties->supportedModes = (1u << ZES_FAN_SPEED_MODE_DEFAULT) | (1u << ZES_FAN_SPEED_MODE_FIXED) | (1u << ZES_FAN_SPEED_MODE_TABLE);
    pProperties->supportedUnits = (1u << ZES_FAN_SPEED_UNITS_RPM) | (1u << ZES_FAN_SPEED_UNITS_PERCENT);

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
        if (maxRpm > 0) {
            pConfig->speedTable.table[pointIndex].speed.speed = (pwmVal * maxRpm) / pwmMax;
            pConfig->speedTable.table[pointIndex].speed.units = ZES_FAN_SPEED_UNITS_RPM;
        } else {
            pConfig->speedTable.table[pointIndex].speed.speed = (pwmVal * 100) / pwmMax;
            pConfig->speedTable.table[pointIndex].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::getConfig(zes_fan_config_t *pConfig) {
    int32_t enableMode = pwmEnableAutoStockTable;
    ze_result_t readResult = pSysfsAccess->read(pwmEnableNode, enableMode);
    if (readResult != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read fan control mode from %s and returning error:0x%x \n", __FUNCTION__, pwmEnableNode.c_str(), readResult);
        return readResult;
    }

    if (enableMode == pwmEnableAutoStockTable) {
        pConfig->mode = ZES_FAN_SPEED_MODE_DEFAULT;
        pConfig->speedFixed = {};
        pConfig->speedTable.numPoints = 0;
        return ZE_RESULT_SUCCESS;
    }

    // pwm_enable = 1 (manual). Distinguish fixed vs table:
    // In fixed mode all maxFanControlPoints auto_point_pwm nodes contain the same value.
    // Only set isFixed if every node is readable and holds the same PWM value.
    int32_t refPwm = 0;
    readResult = pSysfsAccess->read(autoPointPwmNodes[0], refPwm);
    if (readResult != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read fan curve PWM value from %s and returning error:0x%x \n", __FUNCTION__, autoPointPwmNodes[0].c_str(), readResult);
        return readResult;
    }
    bool isFixed = true;

    for (uint32_t i = 1; i < maxFanControlPoints; i++) {
        int32_t pointPwm = 0;
        readResult = pSysfsAccess->read(autoPointPwmNodes[i], pointPwm);
        if (readResult != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read fan curve PWM value from %s and returning error:0x%x \n", __FUNCTION__, autoPointPwmNodes[i].c_str(), readResult);
            return readResult;
        }
        if (pointPwm != refPwm) {
            isFixed = false;
            break;
        }
    }

    if (isFixed) {
        int32_t currentPwm = 0;
        readResult = pSysfsAccess->read(pwmNode, currentPwm);
        if (readResult != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read current fan PWM duty cycle from %s and returning error:0x%x \n", __FUNCTION__, pwmNode.c_str(), readResult);
            return readResult;
        }
        pConfig->mode = ZES_FAN_SPEED_MODE_FIXED;
        pConfig->speedTable.numPoints = 0;
        // maxRpm > 0: fan_max sysfs node was readable and reported a positive value at init,
        // so we can express speed in RPM; otherwise fall back to percent (0-100).
        if (maxRpm > 0) {
            pConfig->speedFixed.speed = (currentPwm * maxRpm) / pwmMax;
            pConfig->speedFixed.units = ZES_FAN_SPEED_UNITS_RPM;
        } else {
            pConfig->speedFixed.speed = (currentPwm * 100) / pwmMax;
            pConfig->speedFixed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        }
        return ZE_RESULT_SUCCESS;
    }

    // Table mode: PWM values differ across auto_point nodes. Count how many
    // temperature curve points the KMD actually exposes by checking which
    // auto_point_temp sysfs nodes exist, then read the full curve.
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
    int32_t rpm = 0;
    ze_result_t readResult = pSysfsAccess->read(fanInputNode, rpm);
    if (readResult != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read current fan speed (RPM) from %s and returning error:0x%x \n", __FUNCTION__, fanInputNode.c_str(), readResult);
        return readResult;
    }

    if (units == ZES_FAN_SPEED_UNITS_RPM) {
        *pSpeed = rpm;
        return ZE_RESULT_SUCCESS;
    }

    // Reject any unit value that is neither RPM nor PERCENT (e.g. FORCE_UINT32).
    if (units != ZES_FAN_SPEED_UNITS_PERCENT) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid fan speed units %d, returning error:0x%x \n", __FUNCTION__, units, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // For PERCENT units : linear mapping using maxRPM (0 RPM = 0%, maxRPM = 100%).
    if (maxRpm <= 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Fan max RPM is not available, returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    const int64_t percent = (static_cast<int64_t>(rpm) * 100) / maxRpm;
    *pSpeed = static_cast<int32_t>(std::clamp<int64_t>(percent, 0, 100));
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::setDefaultMode() {
    ze_result_t result = pSysfsAccess->write(pwmEnableNode, pwmEnableAutoStockTable);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set fan control mode to auto/stock (pwm1_enable=2) via %s and returning error:0x%x \n", __FUNCTION__, pwmEnableNode.c_str(), result);
    }
    return result;
}

ze_result_t LinuxFanImp::setFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    int32_t pwmVal = 0;
    ze_result_t result = speedToPwm(*pSpeed, pwmVal);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): speedToPwm() failed and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    // Switch to manual mode so the direct PWM write takes effect.
    result = pSysfsAccess->write(pwmEnableNode, pwmEnableManualUserTable);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set fan control mode to manual (pwm1_enable=1) via %s and returning error:0x%x \n", __FUNCTION__, pwmEnableNode.c_str(), result);
        return result;
    }

    result = pSysfsAccess->write(pwmNode, pwmVal);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write fixed fan PWM duty cycle (%d) to %s and returning error:0x%x \n", __FUNCTION__, pwmVal, pwmNode.c_str(), result);
    }
    return result;
}

ze_result_t LinuxFanImp::setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    // The table must have at least one point and no more than the KMD-supported maximum
    // (pwm1_auto_point1_* .. pwm1_auto_point10_*).
    if (pSpeedTable->numPoints <= 0 ||
        pSpeedTable->numPoints > static_cast<int32_t>(maxFanControlPoints)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid numPoints %d (valid range: 1..%u), returning error:0x%x \n", __FUNCTION__, pSpeedTable->numPoints, maxFanControlPoints, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Validate and cache the units and speed values for every entry before touching hardware.
    // Both PERCENT and RPM are accepted; speedToPwm() handles the conversion and
    // validates the range (RPM requires fanMaxNode to be present).
    std::vector<int32_t> pwmValues(static_cast<size_t>(pSpeedTable->numPoints));
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        ze_result_t result = speedToPwm(pSpeedTable->table[pointIndex].speed,
                                        pwmValues[static_cast<size_t>(pointIndex)]);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): speedToPwm() failed at point %d and returning error:0x%x \n", __FUNCTION__, pointIndex, result);
            return result;
        }
    }

    // Verify that every required pwm1_auto_point[N]_temp sysfs node exists
    // before writing anything. This prevents a partial update where some points
    // are written and others are not because the KMD exposes fewer nodes than requested.
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        if (!pSysfsAccess->fileExists(autoPointTempNodes[static_cast<uint32_t>(pointIndex)])) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Fan curve temperature node %s does not exist (KMD supports fewer than %d points), returning error:0x%x \n", __FUNCTION__, autoPointTempNodes[static_cast<uint32_t>(pointIndex)].c_str(), pSpeedTable->numPoints, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    // Switch to manual table mode (pwm1_enable = 1) BEFORE writing auto_point values.
    // The KMD rejects writes to pwm1_auto_point*_temp/pwm with EINVAL while the fan
    // is in auto/stock mode (pwm1_enable = 2); manual mode must be active first.
    ze_result_t result = pSysfsAccess->write(pwmEnableNode, pwmEnableManualUserTable);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set fan control mode to manual (pwm1_enable=1) via %s and returning error:0x%x \n", __FUNCTION__, pwmEnableNode.c_str(), result);
        return result;
    }

    // Write each temperature/PWM pair to the corresponding sysfs nodes.
    // Temperature: ZES uses degrees Celsius; the kernel expects millidegrees,
    //              so multiply by milliDegreesPerDegree (1000). e.g. 60C -> 60000.
    // PWM duty:    pre-computed in pwmValues[] above (0-255). e.g. 50% -> 127.
    for (int32_t pointIndex = 0; pointIndex < pSpeedTable->numPoints; pointIndex++) {
        const int32_t tempMilliDeg = pSpeedTable->table[pointIndex].temperature * milliDegreesPerDegree;
        result = pSysfsAccess->write(autoPointTempNodes[static_cast<uint32_t>(pointIndex)], tempMilliDeg);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write fan curve temperature (%d mdeg) to %s and returning error:0x%x \n", __FUNCTION__, tempMilliDeg, autoPointTempNodes[static_cast<uint32_t>(pointIndex)].c_str(), result);
            return result;
        }
        result = pSysfsAccess->write(autoPointPwmNodes[static_cast<uint32_t>(pointIndex)],
                                     pwmValues[static_cast<size_t>(pointIndex)]);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to write fan curve PWM duty cycle (%d) to %s and returning error:0x%x \n", __FUNCTION__, pwmValues[static_cast<size_t>(pointIndex)], autoPointPwmNodes[static_cast<uint32_t>(pointIndex)].c_str(), result);
            return result;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxFanImp::speedToPwm(const zes_fan_speed_t &speed, int32_t &pwmOut) const {
    if (speed.units == ZES_FAN_SPEED_UNITS_PERCENT) {
        if (speed.speed < 0 || speed.speed > 100) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): PERCENT speed %d is out of range [0, 100], returning error:0x%x \n", __FUNCTION__, speed.speed, ZE_RESULT_ERROR_INVALID_ARGUMENT);
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        pwmOut = static_cast<int32_t>((static_cast<int64_t>(speed.speed) * pwmMax) / 100);
        return ZE_RESULT_SUCCESS;
    }
    if (speed.units == ZES_FAN_SPEED_UNITS_RPM) {
        if (speed.speed < 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): RPM speed %d is negative, returning error:0x%x \n", __FUNCTION__, speed.speed, ZE_RESULT_ERROR_INVALID_ARGUMENT);
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (maxRpm <= 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Fan max RPM is not available, returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        int64_t pwm64 = (static_cast<int64_t>(speed.speed) * pwmMax) / maxRpm;
        pwmOut = static_cast<int32_t>(std::min(pwm64, static_cast<int64_t>(pwmMax)));
        return ZE_RESULT_SUCCESS;
    }
    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Unrecognized fan speed units %d, returning error:0x%x \n", __FUNCTION__, speed.units, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
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
    int32_t val = 0;
    maxRpm = -1;
    auto result = pSysfsAccess->read(fanMaxNode, val);
    if (result == ZE_RESULT_SUCCESS && val > 0) {
        maxRpm = val;
    }
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

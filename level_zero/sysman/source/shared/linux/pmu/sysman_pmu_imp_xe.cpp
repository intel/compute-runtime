/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

static ze_result_t getShiftValue(std::string_view readFile, FsAccessInterface *pFsAccess, uint32_t &shiftValue) {

    // The contents of the file passed as an argument is of the form 'config:<start_val>-<end_val>'
    // The start_val is the shift value. It is in decimal format.
    // Eg. if the contents are config:60-63, the shift value in this case is 60

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::string readVal = "";

    result = pFsAccess->read(std::string(readFile), readVal);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    size_t pos = readVal.rfind(":");
    if (pos == std::string::npos) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    readVal = readVal.substr(pos + 1, std::string::npos);
    pos = readVal.rfind("-");
    if (pos == std::string::npos) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    readVal = readVal.substr(0, pos);
    shiftValue = static_cast<uint32_t>(std::strtoul(readVal.c_str(), nullptr, 10));
    return result;
}

int32_t PmuInterfaceImp::getConfigFromEventFile(std::string_view eventFile, uint64_t &config) {

    // The event file from the events directory has following contents
    // event=0x02 --> for /sys/devices/xe_<bdf>/events/engine-active-ticks
    // event=0x03 --> for /sys/devices/xe_<bdf>/events/engine-total-ticks
    // The config values fetched are in hexadecimal format

    auto pFsAccess = pSysmanKmdInterface->getFsAccess();
    std::string readVal = "";
    ze_result_t result = pFsAccess->read(std::string(eventFile), readVal);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }

    size_t pos = readVal.rfind("=");
    if (pos == std::string::npos) {
        return -1;
    }
    readVal = readVal.substr(pos + 1, std::string::npos);
    config = std::strtoul(readVal.c_str(), nullptr, 16);
    return 0;
}

int32_t PmuInterfaceImp::getConfigAfterFormat(std::string_view formatDir, uint64_t &config, uint64_t engineClass, uint64_t engineInstance, uint64_t gt) {

    // The final config is computed by the bitwise OR operation of the config fetched from the event file and value obtained by shifting the parameters gt,
    // engineClass and engineInstance with the shift value fetched from the corresponding file in /sys/devices/xe_<bdf>/format/ directory.
    // The contents of these files in the form of 'config:<start_val>-<end_val>'. For eg.
    // config:60-63 --> for /sys/devices/xe_<bdf>/format/gt
    // config:20-27 --> for /sys/devices/xe_<bdf>/format/engine_class
    // config:12-19 --> for /sys/devices/xe_<bdf>/format/engine_instance
    // The start_val is the shift value by which the parameter should be shifted. The final config is computed as follows.
    // config |= gt << gt_shift_value
    // config |= engineClass << engineClass_shift_value
    // config |= engineInstance << engineInstance_shift_value

    auto pFsAccess = pSysmanKmdInterface->getFsAccess();
    std::string readVal = "";
    uint32_t shiftValue = 0;
    std::string readFile = std::string(formatDir) + "gt";
    ze_result_t result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= gt << shiftValue;

    shiftValue = 0;
    readFile = std::string(formatDir) + "engine_class";
    result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= engineClass << shiftValue;

    shiftValue = 0;
    readFile = std::string(formatDir) + "engine_instance";
    result = getShiftValue(readFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }
    config |= engineInstance << shiftValue;

    return 0;
}

int32_t PmuInterfaceImp::getPmuConfigs(std::string_view sysmanDeviceDir, uint64_t engineClass, uint64_t engineInstance, uint64_t gtId, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) {

    // The PMU configs are first fetched by reading the corresponding values from the event file in /sys/devices/xe_<bdf>/events/ directory and then bitwise ORed with the values obtained by
    // shifting the parameters gt, engineClass and engineInstance with the shift value fetched from the corresponding file in /sys/devices/xe_<bdf>/format/ directory.

    const std::string activeTicksEventFile = std::string(sysmanDeviceDir) + "/events/engine-active-ticks";
    auto ret = getConfigFromEventFile(activeTicksEventFile, activeTicksConfig);
    if (ret < 0) {
        return ret;
    }

    const std::string totalTicksEventFile = std::string(sysmanDeviceDir) + "/events/engine-total-ticks";
    ret = getConfigFromEventFile(totalTicksEventFile, totalTicksConfig);
    if (ret < 0) {
        return ret;
    }

    const std::string formatDir = std::string(sysmanDeviceDir) + "/format/";
    ret = getConfigAfterFormat(formatDir, activeTicksConfig, engineClass, engineInstance, gtId);
    if (ret < 0) {
        return ret;
    }

    ret = getConfigAfterFormat(formatDir, totalTicksConfig, engineClass, engineInstance, gtId);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int32_t PmuInterfaceImp::getPmuConfigsForVf(std::string_view sysmanDeviceDir, uint64_t fnNumber, uint64_t &activeTicksConfig, uint64_t &totalTicksConfig) {

    // The PMU configs for the VFs are fetched by performing bitwise OR of the PMU configs fetched from getPmuConfigs function with the value obtained by shifting the parameter fnNumber with the
    // shift value fetched from /sys/devices/xe_<bdf>/format/function file.

    const std::string functionFile = std::string(sysmanDeviceDir) + "/format/function";
    auto pFsAccess = pSysmanKmdInterface->getFsAccess();

    uint32_t shiftValue = 0;
    ze_result_t result = getShiftValue(functionFile, pFsAccess, shiftValue);
    if (result != ZE_RESULT_SUCCESS) {
        return -1;
    }

    activeTicksConfig |= fnNumber << shiftValue;
    totalTicksConfig |= fnNumber << shiftValue;

    return 0;
}

}; // namespace Sysman
}; // namespace L0
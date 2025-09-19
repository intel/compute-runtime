/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/zes_intel_gpu_sysman.h"
#include <level_zero/zes_api.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/stat.h>
#include <thread>
#if defined(_WIN32) || defined(_WIN64)
#include <shlobj_core.h>
#include <string>
#else // defined(_WIN32) || defined(_WIN64)#
#include <string.h>
#include <unistd.h>
#endif // defined(_WIN32) || defined(_WIN64)
#include <mutex>
#include <vector>
bool verbose = true;

typedef struct {
    zes_firmware_handle_t firmwareHandle;
    std::mutex firmwareProgressMutex;
    bool flashComplete;
} FirmwareFlashInfo;

std::string getErrorString(ze_result_t error) {
    static const std::map<ze_result_t, std::string> mgetErrorString{
        {ZE_RESULT_NOT_READY, "ZE_RESULT_NOT_READY"},
        {ZE_RESULT_ERROR_DEVICE_LOST, "ZE_RESULT_ERROR_DEVICE_LOST"},
        {ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, "ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY"},
        {ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, "ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY"},
        {ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, "ZE_RESULT_ERROR_MODULE_BUILD_FAILURE"},
        {ZE_RESULT_ERROR_MODULE_LINK_FAILURE, "ZE_RESULT_ERROR_MODULE_LINK_FAILURE"},
        {ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, "ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS"},
        {ZE_RESULT_ERROR_NOT_AVAILABLE, "ZE_RESULT_ERROR_NOT_AVAILABLE"},
        {ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, "ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE"},
        {ZE_RESULT_ERROR_UNINITIALIZED, "ZE_RESULT_ERROR_UNINITIALIZED"},
        {ZE_RESULT_ERROR_UNSUPPORTED_VERSION, "ZE_RESULT_ERROR_UNSUPPORTED_VERSION"},
        {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, "ZE_RESULT_ERROR_UNSUPPORTED_FEATURE"},
        {ZE_RESULT_ERROR_INVALID_ARGUMENT, "ZE_RESULT_ERROR_INVALID_ARGUMENT"},
        {ZE_RESULT_ERROR_INVALID_NULL_HANDLE, "ZE_RESULT_ERROR_INVALID_NULL_HANDLE"},
        {ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, "ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE"},
        {ZE_RESULT_ERROR_INVALID_NULL_POINTER, "ZE_RESULT_ERROR_INVALID_NULL_POINTER"},
        {ZE_RESULT_ERROR_INVALID_SIZE, "ZE_RESULT_ERROR_INVALID_SIZE"},
        {ZE_RESULT_ERROR_UNSUPPORTED_SIZE, "ZE_RESULT_ERROR_UNSUPPORTED_SIZE"},
        {ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT, "ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT"},
        {ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT, "ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT"},
        {ZE_RESULT_ERROR_INVALID_ENUMERATION, "ZE_RESULT_ERROR_INVALID_ENUMERATION"},
        {ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, "ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION"},
        {ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT, "ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT"},
        {ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, "ZE_RESULT_ERROR_INVALID_NATIVE_BINARY"},
        {ZE_RESULT_ERROR_INVALID_GLOBAL_NAME, "ZE_RESULT_ERROR_INVALID_GLOBAL_NAME"},
        {ZE_RESULT_ERROR_INVALID_KERNEL_NAME, "ZE_RESULT_ERROR_INVALID_KERNEL_NAME"},
        {ZE_RESULT_ERROR_INVALID_FUNCTION_NAME, "ZE_RESULT_ERROR_INVALID_FUNCTION_NAME"},
        {ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, "ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION"},
        {ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION, "ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION"},
        {ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX, "ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX"},
        {ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE, "ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE"},
        {ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE, "ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE"},
        {ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED, "ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED"},
        {ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE, "ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE"},
        {ZE_RESULT_ERROR_OVERLAPPING_REGIONS, "ZE_RESULT_ERROR_OVERLAPPING_REGIONS"},
        {ZE_RESULT_ERROR_UNKNOWN, "ZE_RESULT_ERROR_UNKNOWN"}};
    auto i = mgetErrorString.find(error);
    if (i == mgetErrorString.end())
        return "ZE_RESULT_ERROR_UNKNOWN";
    else
        return mgetErrorString.at(error);
}
inline bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName, int *optind) {
    if (argc < 2) {
        return false;
    }
    int index = 1;
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            *optind = index;
            return true;
        }
        index++;
    }

    return false;
}

#define VALIDATECALL(myZeCall)                \
    do {                                      \
        ze_result_t r = myZeCall;             \
        if (r != ZE_RESULT_SUCCESS) {         \
            std::cout << getErrorString(r)    \
                      << " returned by "      \
                      << #myZeCall << ": "    \
                      << __FUNCTION__ << ": " \
                      << __LINE__ << "\n";    \
        }                                     \
    } while (0);

void usage() {
    std::cout << "\n"
                 "\n zello_sysman [OPTIONS]"
                 "\n"
                 "\n OPTIONS:"
                 "\n  -p,   --pci                                                                       selectively run pci black box test"
                 "\n  -f,   --frequency                                                                 selectively run frequency black box test"
                 "\n  -s,   --standby                                                                   selectively run standby black box test"
                 "\n  -e,   --engine                                                                    selectively run engine black box test"
                 "\n  -c,   --scheduler                                                                 selectively run scheduler black box test"
                 "\n  -t,   --temperature                                                               selectively run temperature black box test"
                 "\n  -o,   --power                                                                     selectively run power black box test"
                 "\n        [--setlimit --sustained/--peak/--instantaneous/--burst <deviceNo limit>]    optionally set required power limit for particular device"
                 "\n  -m,   --memory                                                                    selectively run memory black box test"
                 "\n  -g,   --global                                                                    selectively run device/global operations black box test"
                 "\n  -R,   --ras                                                                       selectively run ras black box test"
                 "\n  -E,   --event                                                                     set and listen to events black box test"
                 "\n  -r,   --reset force|noforce                                                       selectively run device reset test on all devices"
                 "\n        [deviceNo]                                                                  optionally run device reset test only on specified device"
                 "\n  -i,   --firmware <image>                                                          selectively run device firmware test <image> is the firmware binary needed to flash"
                 "\n  -F,   --fabricport                                                                selectively run fabricport black box test"
                 "\n  -d,   --diagnostics                                                               selectively run diagnostics black box test"
                 "\n  -P,   --performance                                                               selectively run performance black box test"
                 "\n        [--setconfig <deviceNo subdevId engineFlags pFactor>]                       optionally set the performance factor for the particular handle"
                 "\n  -C,   --ecc                                                                       selectively run ecc black box test"
                 "\n  -a,   --fan                                                                       selectively run fan black box test"
                 "\n  -h,   --help                                                                      display help message"
                 "\n  -re,  --rasexp                                                                    selectively run ras experimental API black box test"
                 "\n  -v,   --vftelemetry                                                               selectively run vf telemetry API black box test"
                 "\n"
                 "\n  All L0 Syman APIs that set values require root privileged execution"
                 "\n"
                 "\n";
}

void getDeviceHandles(ze_driver_handle_t &driverHandle, std::vector<ze_device_handle_t> &devices) {
    VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    uint32_t driverCount = 0;
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));
    if (driverCount == 0) {
        std::cout << "Error could not retrieve driver" << std::endl;
        std::terminate();
    }
    VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));

    uint32_t deviceCount = 0;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cout << "Error could not retrieve device" << std::endl;
        std::terminate();
    }
    devices.resize(deviceCount);
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    for (const auto &device : devices) {
        VALIDATECALL(zeDeviceGetProperties(device, &deviceProperties));
        if (verbose) {
            std::cout << "Device Name = " << deviceProperties.name << std::endl;
            std::cout << "UUID: " << std::endl;
            for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
                std::cout << +deviceProperties.uuid.id[i] << " ";
            }
            std::cout << std::endl;
        }
    }
}

void getSysmanDeviceHandles(zes_driver_handle_t &sysmanDriverHandle, std::vector<zes_device_handle_t> &sysmanDevices) {
    VALIDATECALL(zesInit(0));

    uint32_t driverCount = 0;
    VALIDATECALL(zesDriverGet(&driverCount, nullptr));
    if (driverCount == 0) {
        std::cout << "Error could not retrieve driver" << std::endl;
        std::terminate();
    }
    VALIDATECALL(zesDriverGet(&driverCount, &sysmanDriverHandle));

    uint32_t deviceCount = 0;
    VALIDATECALL(zesDeviceGet(sysmanDriverHandle, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cout << "Error could not retrieve device" << std::endl;
        std::terminate();
    }
    sysmanDevices.resize(deviceCount);
    VALIDATECALL(zesDeviceGet(sysmanDriverHandle, &deviceCount, sysmanDevices.data()));
}

std::string getPowerLimitSourceType(zes_power_source_t type) {
    static const std::map<zes_power_source_t, std::string> powerLimitsSourceTypeMap{
        {ZES_POWER_SOURCE_ANY, "ZES_POWER_SOURCE_ANY"},
        {ZES_POWER_SOURCE_MAINS, "ZES_POWER_SOURCE_MAINS"},
        {ZES_POWER_SOURCE_BATTERY, "ZES_POWER_SOURCE_BATTERY"}};
    return powerLimitsSourceTypeMap.at(type);
}

void getPowerLimits(const zes_pwr_handle_t &handle) {
    uint32_t limitCount = 0;
    VALIDATECALL(zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    if (limitCount == 0) {
        std::cout << "powerLimitDesc.count = " << limitCount << std::endl;
    } else {
        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        VALIDATECALL(zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        if (verbose) {
            for (uint32_t i = 0; i < limitCount; i++) {
                switch (allLimits[i].level) {
                case ZES_POWER_LEVEL_SUSTAINED:
                    std::cout << " --- Sustained Power Limit --- " << std::endl;
                    break;
                case ZES_POWER_LEVEL_PEAK:
                    std::cout << " --- Peak Power Limit --- " << std::endl;
                    break;
                case ZES_POWER_LEVEL_BURST:
                    std::cout << " --- Burst Power Limit --- " << std::endl;
                    break;
                case ZES_POWER_LEVEL_INSTANTANEOUS:
                    std::cout << " --- Instantaneous Power Limit --- " << std::endl;
                    break;
                default:
                    std::cout << " --- Invalid Power Limit --- " << std::endl;
                    return;
                }

                std::cout << "powerLimit.intervalValueLocked = " << +allLimits[i].intervalValueLocked << std::endl;
                std::cout << "powerLimit.enabledStateLocked = " << +allLimits[i].enabledStateLocked << std::endl;
                std::cout << "powerLimit.limitValueLocked = " << +allLimits[i].limitValueLocked << std::endl;
                std::cout << "powerLimit.source = " << getPowerLimitSourceType(allLimits[i].source) << std::endl;
                std::cout << "powerLimit.limitUnit = " << allLimits[i].limitUnit << std::endl;
                std::cout << "powerLimit.limit = " << allLimits[i].limit << std::endl;
                std::cout << "powerLimit.interval = " << allLimits[i].interval << std::endl;
            }
        }
    }
}

void setPowerLimit(const zes_pwr_handle_t &handle, std::vector<std::string> &buf) {
    zes_power_level_t level;
    uint32_t limitCount = 0;
    ze_bool_t isPowerLevelAvailable = false;

    if (buf[1] == "--sustained") {
        level = ZES_POWER_LEVEL_SUSTAINED;
    } else if (buf[1] == "--peak") {
        level = ZES_POWER_LEVEL_PEAK;
    } else if (buf[1] == "--instantaneous") {
        level = ZES_POWER_LEVEL_INSTANTANEOUS;
    } else {
        level = ZES_POWER_LEVEL_BURST;
    }

    VALIDATECALL(zesPowerGetLimitsExt(handle, &limitCount, nullptr));
    if (limitCount != 0) {
        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        VALIDATECALL(zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == level) {
                allLimits[i].limit = static_cast<int32_t>(std::stoi(buf[3]));
                isPowerLevelAvailable = true;
                break;
            }
        }

        if (isPowerLevelAvailable) {
            VALIDATECALL(zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
        } else {
            std::cout << "Unsupported Power Level to set limit" << std::endl;
        }
    } else {
        std::cout << "Unsupported Power Level to set limit" << std::endl;
    }
}
#if defined(_WIN32) || defined(_WIN64)
int geteuid() {
    if (IsUserAnAdmin()) {
        return 0;
    } else {
        return -1;
    }
}
#endif // defined(_WIN32) || defined(_WIN64)
static std::string getPowerDomainType(zes_power_domain_t type) {
    static const std::map<zes_power_domain_t, std::string> powerDomainEnumToStringMap{
        {ZES_POWER_DOMAIN_CARD, "ZES_POWER_DOMAIN_CARD"},
        {ZES_POWER_DOMAIN_PACKAGE, "ZES_POWER_DOMAIN_PACKAGE"},
        {ZES_POWER_DOMAIN_STACK, "ZES_POWER_DOMAIN_STACK"},
        {ZES_POWER_DOMAIN_MEMORY, "ZES_POWER_DOMAIN_MEMORY"},
        {ZES_POWER_DOMAIN_GPU, "ZES_POWER_DOMAIN_GPU"}};
    if (powerDomainEnumToStringMap.find(type) != powerDomainEnumToStringMap.end()) {
        return powerDomainEnumToStringMap.at(type);
    } else {
        return "invalid";
    }
}

void testSysmanPower(ze_device_handle_t &device, std::vector<std::string> &buf, uint32_t &curDeviceIndex) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << std::endl
              << " ----  Power tests ---- " << std::endl;
    bool iamroot = (geteuid() == 0);
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumPowerDomains(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Power domains" << std::endl;
        return;
    }
    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumPowerDomains(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};

        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;
        VALIDATECALL(zesPowerGetProperties(handle, &properties));
        if (verbose) {
            std::cout << "properties.onSubdevice = " << static_cast<uint32_t>(properties.onSubdevice) << std::endl;
            std::cout << "properties.subdeviceId = " << properties.subdeviceId << std::endl;
            std::cout << "properties.canControl = " << static_cast<uint32_t>(properties.canControl) << std::endl;
            std::cout << "properties.isEnergyThresholdSupported = " << static_cast<uint32_t>(properties.isEnergyThresholdSupported) << std::endl;
            std::cout << "properties.defaultLimit = " << properties.defaultLimit << std::endl;
            std::cout << "properties.maxLimit = " << properties.maxLimit << std::endl;
            std::cout << "properties.minLimit = " << properties.minLimit << std::endl;
            std::cout << "extProperties.domain = " << getPowerDomainType(extProperties.domain) << std::endl;
            std::cout << "defaultLimit.limit = " << defaultLimit.limit << std::endl;
        }
        int count = 5; // Measure average power 5 times
        for (; count > 0; count--) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            zes_power_energy_counter_t energyCounter1;
            VALIDATECALL(zesPowerGetEnergyCounter(handle, &energyCounter1));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            zes_power_energy_counter_t energyCounter2;
            VALIDATECALL(zesPowerGetEnergyCounter(handle, &energyCounter2));
            auto deltaE = static_cast<float>(energyCounter2.energy - energyCounter1.energy);
            auto deltaT = static_cast<float>(energyCounter2.timestamp - energyCounter1.timestamp);
            float powerWatt = deltaE / deltaT;

            if (verbose) {
                std::string deviceType;
                if (static_cast<uint32_t>(properties.onSubdevice)) {
                    deviceType = "subDevice";
                } else {
                    deviceType = "rootDevice";
                }
                std::cout << "CurrentPower = " << powerWatt << " W for" << deviceType << std::endl;
            }
        }

        if (!properties.onSubdevice) {
            zes_power_sustained_limit_t sustainedGetDefault = {};
            zes_power_peak_limit_t peakGetDefault = {};
            VALIDATECALL(zesPowerGetLimits(handle, &sustainedGetDefault, nullptr, &peakGetDefault));
            if (iamroot) {
                VALIDATECALL(zesPowerSetLimits(handle, &sustainedGetDefault, nullptr, &peakGetDefault));
            }
            if (buf.size() != 0) {
                uint32_t deviceIndex = static_cast<uint32_t>(std::stoi(buf[2]));
                if (deviceIndex == curDeviceIndex) {
                    if (iamroot) {
                        setPowerLimit(handle, buf);
                    } else {
                        std::cout << "In sufficient permissions to set power limit" << std::endl;
                    }
                }
            }
            getPowerLimits(handle);
        }
    }
    curDeviceIndex++;
}

std::string getEngineFlagType(zes_engine_type_flags_t engineFlag) {
    static const std::map<zes_engine_type_flags_t, std::string> mgetEngineType{
        {ZES_ENGINE_TYPE_FLAG_OTHER, "ZES_ENGINE_TYPE_FLAG_OTHER"},
        {ZES_ENGINE_TYPE_FLAG_COMPUTE, "ZES_ENGINE_TYPE_FLAG_COMPUTE"},
        {ZES_ENGINE_TYPE_FLAG_3D, "ZES_ENGINE_TYPE_FLAG_3D"},
        {ZES_ENGINE_TYPE_FLAG_MEDIA, "ZES_ENGINE_TYPE_FLAG_MEDIA"},
        {ZES_ENGINE_TYPE_FLAG_DMA, "ZES_ENGINE_TYPE_FLAG_DMA"},
        {ZES_ENGINE_TYPE_FLAG_RENDER, "ZES_ENGINE_TYPE_FLAG_RENDER"}};
    auto i = mgetEngineType.find(engineFlag);
    if (i == mgetEngineType.end())
        return "No supported engine type flag available";
    else
        return mgetEngineType.at(engineFlag);
}

zes_engine_type_flags_t getEngineFlagType(std::string engineFlagString) {
    static const std::map<std::string, zes_engine_type_flags_t> mgetEngineType{
        {"ZES_ENGINE_TYPE_FLAG_OTHER", ZES_ENGINE_TYPE_FLAG_OTHER},
        {"ZES_ENGINE_TYPE_FLAG_COMPUTE", ZES_ENGINE_TYPE_FLAG_COMPUTE},
        {"ZES_ENGINE_TYPE_FLAG_3D", ZES_ENGINE_TYPE_FLAG_3D},
        {"ZES_ENGINE_TYPE_FLAG_MEDIA", ZES_ENGINE_TYPE_FLAG_MEDIA},
        {"ZES_ENGINE_TYPE_FLAG_DMA", ZES_ENGINE_TYPE_FLAG_DMA},
        {"ZES_ENGINE_TYPE_FLAG_RENDER", ZES_ENGINE_TYPE_FLAG_RENDER}};
    auto i = mgetEngineType.find(engineFlagString);
    if (i == mgetEngineType.end()) {
        std::cout << "Engine type flag Unsupported" << std::endl;
        return ZES_ENGINE_TYPE_FLAG_FORCE_UINT32;
    } else
        return mgetEngineType.at(engineFlagString);
}

void setPerformanceFactor(const zes_perf_handle_t &handle, const zes_perf_properties_t &properties, std::vector<std::string> &buf, bool &pFactorIsSet) {
    uint32_t subdeviceId = static_cast<uint32_t>(std::stoi(buf[2]));
    zes_engine_type_flags_t engineTypeFlag = getEngineFlagType(buf[3]);
    double pFactor = static_cast<uint32_t>(std::stod(buf[4]));
    if (properties.subdeviceId == subdeviceId && properties.engines == engineTypeFlag) {
        if (properties.engines == engineTypeFlag) {
            VALIDATECALL(zesPerformanceFactorSetConfig(handle, pFactor));
            std::cout << "Performance factor is set successfully" << std::endl;
            pFactorIsSet = true;
        }
    }
}

void testSysmanPerformance(ze_device_handle_t &device, std::vector<std::string> &buf, uint32_t &curDeviceIndex, bool &pFactorIsSet) {
    std::cout << std::endl
              << " ----  Performance-factor tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumPerformanceFactorDomains(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Performance factor domains" << std::endl;
        return;
    }
    std::vector<zes_perf_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumPerformanceFactorDomains(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_perf_properties_t properties;
        VALIDATECALL(zesPerformanceFactorGetProperties(handle, &properties));
        if (verbose) {
            std::cout << "properties.onSubdevice = " << static_cast<uint32_t>(properties.onSubdevice) << std::endl;
            std::cout << "properties.subdeviceId = " << properties.subdeviceId << std::endl;
            std::cout << "properties.engines = " << getEngineFlagType(properties.engines) << std::endl;
        }
        if (buf.size() != 0) {
            uint32_t deviceIndex = static_cast<uint32_t>(std::stoi(buf[1]));
            if (deviceIndex == curDeviceIndex) {
                setPerformanceFactor(handle, properties, buf, pFactorIsSet);
            }
        }
        double originalFactor = 0;
        VALIDATECALL(zesPerformanceFactorGetConfig(handle, &originalFactor));
        if (verbose) {
            std::cout << "current Performance Factor = " << std::round(originalFactor) << std::endl;
        }
        std::cout << std::endl;
    }
    curDeviceIndex++;
}

std::string getTemperatureSensorType(zes_temp_sensors_t type) {
    static const std::map<zes_temp_sensors_t, std::string> mgetSensorType{
        {ZES_TEMP_SENSORS_GLOBAL, "ZES_TEMP_SENSORS_GLOBAL"},
        {ZES_TEMP_SENSORS_GPU, "ZES_TEMP_SENSORS_GPU"},
        {ZES_TEMP_SENSORS_MEMORY, "ZES_TEMP_SENSORS_MEMORY"},
        {ZES_TEMP_SENSORS_GLOBAL_MIN, "ZES_TEMP_SENSORS_GLOBAL_MIN"},
        {ZES_TEMP_SENSORS_GPU_MIN, "ZES_TEMP_SENSORS_GPU_MIN"},
        {ZES_TEMP_SENSORS_MEMORY_MIN, "ZES_TEMP_SENSORS_MEMORY_MIN"}};
    auto i = mgetSensorType.find(type);
    if (i == mgetSensorType.end())
        return "No supported temperature type available";
    else
        return mgetSensorType.at(type);
}

void testSysmanTemperature(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Temperature tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumTemperatureSensors(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Temperature domains" << std::endl;
        return;
    }
    std::vector<zes_temp_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumTemperatureSensors(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_temp_properties_t properties = {};
        VALIDATECALL(zesTemperatureGetProperties(handle, &properties));

        double temperature;
        VALIDATECALL(zesTemperatureGetState(handle, &temperature));
        if (verbose) {
            std::cout << "For subDevice " << properties.subdeviceId << " temperature current state for "
                      << getTemperatureSensorType(properties.type) << " is: " << temperature << std::endl;
        }
    }
}

void testSysmanEcc(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Ecc tests ---- " << std::endl;

    ze_bool_t eccAvailable = false;
    VALIDATECALL(zesDeviceEccAvailable(device, &eccAvailable));
    if (eccAvailable == false) {
        std::cout << "Ecc not available" << std::endl;
        return;
    }

    ze_bool_t eccConfigurable = false;
    VALIDATECALL(zesDeviceEccConfigurable(device, &eccConfigurable));
    if (eccConfigurable == false) {
        std::cout << "Ecc not configurable" << std::endl;
        return;
    }

    zes_device_ecc_properties_t getProps = {};
    zes_device_ecc_default_properties_ext_t extProps = {};
    extProps.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT;
    getProps.pNext = &extProps;
    VALIDATECALL(zesDeviceGetEccState(device, &getProps));
    if (verbose) {
        std::cout << "getStateProps.pendingState " << getProps.pendingState << std::endl;
        std::cout << "getStateProps.currentState " << getProps.currentState << std::endl;
        std::cout << "getStateProps.defaultState " << extProps.defaultState << std::endl;
        std::cout << "getStateProps.pendingAction " << getProps.pendingAction << std::endl;
    }

    if (verbose) {
        std::cout << "Setting Ecc state to " << ZES_DEVICE_ECC_STATE_ENABLED << std::endl;
    }
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_ECC_DESC, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t setProps = {};
    VALIDATECALL(zesDeviceSetEccState(device, &newState, &setProps));
    if (verbose) {
        std::cout << "setStateProps.pendingState " << setProps.pendingState << std::endl;
        std::cout << "setStateProps.currentState " << setProps.currentState << std::endl;
        std::cout << "setStateProps.pendingAction " << setProps.pendingAction << std::endl;
    }

    if (verbose) {
        std::cout << "Setting Ecc state to " << ZES_DEVICE_ECC_STATE_DISABLED << std::endl;
    }
    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    VALIDATECALL(zesDeviceSetEccState(device, &newState, &setProps));
    if (verbose) {
        std::cout << "setStateProps.pendingState " << setProps.pendingState << std::endl;
        std::cout << "setStateProps.currentState " << setProps.currentState << std::endl;
        std::cout << "setStateProps.pendingAction " << setProps.pendingAction << std::endl;
    }

    // Restore to original state
    if (setProps.pendingState != getProps.pendingState) {
        if (verbose) {
            std::cout << "Restoring Ecc configuration to original state " << std::endl;
            newState.state = getProps.pendingState;
        }
        VALIDATECALL(zesDeviceSetEccState(device, &newState, &setProps));
    }
}

void testSysmanSurvivability(ze_device_handle_t &device) {
    zes_device_properties_t properties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_result_t result = zesDeviceGetProperties(device, &properties);
    if (result == ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED) {
        std::cout << "Device is in survivability mode!!, Only firmware update supported" << std::endl;
    }
    if ((result == ZE_RESULT_SUCCESS) && verbose) {
        std::cout << "Device is in Normal operations Mode. Device properties retrieved successfully. " << std::endl;
        std::cout << "Device Name = " << properties.core.name << std::endl;
        std::cout << "properties.vendorName = " << properties.vendorName << std::endl;
        std::cout << "properties.core.vendorId = " << properties.core.vendorId << std::endl;
        std::cout << "properties.core.deviceId = " << properties.core.deviceId << std::endl;
        std::cout << "properties.core.uuid = " << std::endl;
        for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
            std::cout << +properties.core.uuid.id[i] << " ";
        }
        std::cout << std::endl;
        return;
    }
    uint32_t count = 0;
    result = zesDeviceEnumFirmwares(device, &count, nullptr);
    if (result != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceEnumFirmwares() Failed!!" << std::endl;
        return;
    }
    if (count == 0) {
        std::cout << "Could not retrieve Firmware domains" << std::endl;
        return;
    } else {
        std::cout << "Found " << count << " firmware handles.." << std::endl;
    }

    std::vector<zes_firmware_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumFirmwares(device, &count, handles.data()));

    for (auto handle : handles) {
        zes_firmware_properties_t fwProperties = {};

        VALIDATECALL(zesFirmwareGetProperties(handle, &fwProperties));
        if (verbose) {
            std::cout << "firmware name = " << fwProperties.name << std::endl;
            std::cout << "On Subdevice = " << static_cast<uint32_t>(fwProperties.onSubdevice) << std::endl;
            std::cout << "Subdevice Id = " << fwProperties.subdeviceId << std::endl;
            std::cout << "firmware version = " << fwProperties.version << std::endl;
        }
    }
}

void testSysmanPci(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  PCI tests ---- " << std::endl;
    zes_pci_properties_t properties = {};
    zes_intel_pci_link_speed_downgrade_exp_properties_t extProps = {};
    extProps.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES;
    properties.pNext = &extProps;
    VALIDATECALL(zesDevicePciGetProperties(device, &properties));
    if (verbose) {
        std::cout << "properties.address.domain = " << std::hex << properties.address.domain << std::endl;
        std::cout << "properties.address.bus = " << std::hex << properties.address.bus << std::endl;
        std::cout << "properties.address.device = " << std::hex << properties.address.device << std::endl;
        std::cout << "properties.address.function = " << std::hex << properties.address.function << std::endl;
        std::cout << "properties.maxSpeed.gen = " << std::dec << properties.maxSpeed.gen << std::endl;
        std::cout << "properties.maxSpeed.width = " << std::dec << properties.maxSpeed.width << std::endl;
        std::cout << "properties.maxSpeed.maxBandwidth = " << std::dec << properties.maxSpeed.maxBandwidth << std::endl;
        std::cout << "properties.haveBandwidthCounters = " << static_cast<uint32_t>(properties.haveBandwidthCounters) << std::endl;
        std::cout << "properties.havePacketCounters = " << static_cast<uint32_t>(properties.havePacketCounters) << std::endl;
        std::cout << "properties.haveReplayCounters = " << static_cast<uint32_t>(properties.haveReplayCounters) << std::endl;
        std::cout << "properties.pciLinkSpeedUpdateCapable = " << static_cast<bool>(extProps.pciLinkSpeedUpdateCapable) << std::endl;
        std::cout << "properties.maxPciGenSupported = " << static_cast<int32_t>(extProps.maxPciGenSupported) << std::endl;
    }

    uint32_t count = 0;
    VALIDATECALL(zesDevicePciGetBars(device, &count, nullptr));
    if (verbose) {
        std::cout << "Bar count = " << count << std::endl;
    }

    std::vector<zes_pci_bar_properties_t> pciBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> pciBarExtProps(count);
    for (uint32_t i = 0; i < count; i++) {
        pciBarExtProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2;
        pciBarExtProps[i].pNext = nullptr;
        pciBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pciBarProps[i].pNext = static_cast<void *>(&pciBarExtProps[i]);
    }

    zes_pci_stats_t pciStats = {};
    VALIDATECALL(zesDevicePciGetStats(device, &pciStats));
    if (verbose) {
        if (properties.haveReplayCounters) {
            std::cout << "pciStats.replayCounter = " << std::dec << pciStats.replayCounter << std::endl;
        }
        if (properties.havePacketCounters) {
            std::cout << "pciStats.packetCounter = " << std::dec << pciStats.packetCounter << std::endl;
        }
        if (properties.haveBandwidthCounters) {
            std::cout << "pciStats.rxCounter = " << std::dec << pciStats.rxCounter << std::endl;
            std::cout << "pciStats.txCounter = " << std::dec << pciStats.txCounter << std::endl;
        }
        std::cout << "pciStats.timestamp = " << std::dec << pciStats.timestamp << std::endl;
        std::cout << "pciStats.speed.gen = " << std::dec << pciStats.speed.gen << std::endl;
        std::cout << "pciStats.speed.width = " << std::dec << pciStats.speed.width << std::endl;
        std::cout << "pciStats.speed.maxBandWidth = " << std::dec << pciStats.speed.maxBandwidth << std::endl;
    }

    VALIDATECALL(zesDevicePciGetBars(device, &count, pciBarProps.data()));
    if (verbose) {
        for (uint32_t i = 0; i < count; i++) {
            std::cout << "pciBarProps.type = " << std::hex << pciBarProps[i].type << std::endl;
            std::cout << "pciBarProps.index = " << std::hex << pciBarProps[i].index << std::endl;
            std::cout << "pciBarProps.base = " << std::hex << pciBarProps[i].base << std::endl;
            std::cout << "pciBarProps.size = " << std::hex << pciBarProps[i].size << std::endl;
            std::cout << "pci_bar_properties_1_2_t.resizableBarSupported = " << static_cast<uint32_t>(pciBarExtProps[i].resizableBarSupported) << std::endl;
            std::cout << "pci_bar_properties_1_2_t.resizableBarEnabled = " << static_cast<uint32_t>(pciBarExtProps[i].resizableBarEnabled) << std::endl;
        }
    }

    zes_pci_state_t pciState = {};
    zes_intel_pci_link_speed_downgrade_exp_state_t extState = {};
    extState.stype = ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE;
    pciState.pNext = &extState;
    VALIDATECALL(zesDevicePciGetState(device, &pciState));
    if (verbose) {
        std::cout << "pciState.speed.gen = " << std::dec << pciState.speed.gen << std::endl;
        std::cout << "pciState.speed.width = " << std::dec << pciState.speed.width << std::endl;
        std::cout << "pciState.speed.maxBandWidth = " << std::dec << pciState.speed.maxBandwidth << std::endl;
        std::cout << "pciState.pciLinkSpeedDowngradeStatus = " << static_cast<bool>(extState.pciLinkSpeedDowngradeStatus) << std::endl;
    }
}

void testSysmanFrequency(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Frequency tests ---- " << std::endl;
    bool iamroot = (geteuid() == 0);

    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumFrequencyDomains(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve frequency domains" << std::endl;
        return;
    }
    std::vector<zes_freq_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumFrequencyDomains(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_freq_properties_t freqProperties = {};
        zes_freq_range_t freqRange = {};
        zes_freq_range_t testFreqRange = {};
        zes_freq_state_t freqState = {};

        VALIDATECALL(zesFrequencyGetProperties(handle, &freqProperties));
        if (verbose) {
            std::cout << "freqProperties.type = " << freqProperties.type << std::endl;
            std::cout << "freqProperties.canControl = " << static_cast<uint32_t>(freqProperties.canControl) << std::endl;
            std::cout << "freqProperties.isThrottleEventSupported = " << static_cast<uint32_t>(freqProperties.isThrottleEventSupported) << std::endl;
            std::cout << "freqProperties.min = " << freqProperties.min << std::endl;
            std::cout << "freqProperties.max = " << freqProperties.max << std::endl;
            if (freqProperties.onSubdevice) {
                std::cout << "freqProperties.subdeviceId = " << freqProperties.subdeviceId << std::endl;
            }
        }

        VALIDATECALL(zesFrequencyGetState(handle, &freqState));
        if (verbose) {
            std::cout << "freqState.currentVoltage = " << freqState.currentVoltage << std::endl;
            std::cout << "freqState.request = " << freqState.request << std::endl;
            std::cout << "freqState.tdp = " << freqState.tdp << std::endl;
            std::cout << "freqState.efficient = " << freqState.efficient << std::endl;
            std::cout << "freqState.actual = " << freqState.actual << std::endl;
            std::cout << "freqState.throttleReasons = " << freqState.throttleReasons << std::endl;
        }

        VALIDATECALL(zesFrequencyGetRange(handle, &freqRange));
        if (verbose) {
            std::cout << "freqRange.min = " << freqRange.min << std::endl;
            std::cout << "freqRange.max = " << freqRange.max << std::endl;
        }
        count = 0;
        VALIDATECALL(zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        std::vector<double> frequency(count);
        VALIDATECALL(zesFrequencyGetAvailableClocks(handle, &count, frequency.data()));
        if (verbose) {
            for (auto freq : frequency) {
                std::cout << " frequency = " << freq << std::endl;
            }
        }
        if (iamroot) {
            // Test setting min and max frequency the same, then restore originals
            testFreqRange.min = freqRange.min;
            testFreqRange.max = freqRange.min;
            if (verbose) {
                std::cout << "Setting Frequency Range . min " << testFreqRange.min << std::endl;
                std::cout << "Setting Frequency Range . max " << testFreqRange.max << std::endl;
            }
            VALIDATECALL(zesFrequencySetRange(handle, &testFreqRange));
            VALIDATECALL(zesFrequencyGetRange(handle, &testFreqRange));
            if (verbose) {
                std::cout << "After Setting Getting Frequency Range . min " << testFreqRange.min << std::endl;
                std::cout << "After Setting Getting Frequency Range . max " << testFreqRange.max << std::endl;
            }
            testFreqRange.min = freqRange.min;
            testFreqRange.max = freqRange.max;
            if (verbose) {
                std::cout << "Setting Frequency Range . min " << testFreqRange.min << std::endl;
                std::cout << "Setting Frequency Range . max " << testFreqRange.max << std::endl;
            }
            VALIDATECALL(zesFrequencySetRange(handle, &testFreqRange));
            VALIDATECALL(zesFrequencyGetRange(handle, &testFreqRange));
            if (verbose) {
                std::cout << "After Setting Getting Frequency Range . min " << testFreqRange.min << std::endl;
                std::cout << "After Setting Getting Frequency Range . max " << testFreqRange.max << std::endl;
            }
        } else {
            std::cout << "Not running as Root. Skipping zetSysmanFrequencySetRange test." << std::endl;
        }
    }
}

void testSysmanRasExp(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Ras Exp tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumRasErrorSets(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Ras Error Sets" << std::endl;
        return;
    }
    std::vector<zes_ras_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumRasErrorSets(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_ras_properties_t rasProperties = {};

        VALIDATECALL(zesRasGetProperties(handle, &rasProperties));
        if (verbose) {
            std::cout << "rasProperties.type = " << rasProperties.type << std::endl;
            if (rasProperties.onSubdevice) {
                std::cout << "rasProperties.subdeviceId = " << rasProperties.subdeviceId << std::endl;
            }
        }

        // Query for number of error categories supported by platform
        uint32_t rasCategoryCount = 0;
        VALIDATECALL(zesRasGetStateExp(handle, &rasCategoryCount, nullptr));

        // Gather error states
        std::vector<zes_ras_state_exp_t> rasStates(rasCategoryCount);
        VALIDATECALL(zesRasGetStateExp(handle, &rasCategoryCount, rasStates.data()));

        if (verbose) {
            if (rasProperties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                std::cout << "Uncorrectable errors listing:" << std::endl;
            } else {
                std::cout << "Correctable errors listing:" << std::endl;
            }
            for (uint32_t i = 0; i < rasCategoryCount; i++) {
                std::cout << " Error category: " << rasStates[i].category << "Count: " << rasStates[i].errorCounter << std::endl;
            }
        }
    }
}

void testSysmanRas(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Ras tests ---- " << std::endl;
    uint32_t count = 0;
    bool iamroot = (geteuid() == 0);
    VALIDATECALL(zesDeviceEnumRasErrorSets(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Ras Error Sets" << std::endl;
        return;
    }
    std::vector<zes_ras_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumRasErrorSets(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_ras_properties_t rasProperties = {};
        zes_ras_state_t rasState = {};

        VALIDATECALL(zesRasGetProperties(handle, &rasProperties));
        if (verbose) {
            std::cout << "rasProperties.type = " << rasProperties.type << std::endl;
            if (rasProperties.onSubdevice) {
                std::cout << "rasProperties.subdeviceId = " << rasProperties.subdeviceId << std::endl;
            }
        }
        ze_bool_t clear = 0;
        VALIDATECALL(zesRasGetState(handle, clear, &rasState));
        if (verbose) {
            if (rasProperties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                std::cout << "Number of fatal accelerator engine resets attempted by the driver = " << rasState.category[ZES_RAS_ERROR_CAT_RESET] << std::endl;
                std::cout << "Number of fatal errors that have occurred in caches = " << rasState.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS] << std::endl;
                std::cout << "Number of fatal programming errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS] << std::endl;
                std::cout << "Number of fatal driver errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS] << std::endl;
                std::cout << "Number of fatal compute errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS] << std::endl;
                std::cout << "Number of fatal non compute errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS] << std::endl;
                std::cout << "Number of fatal display errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS] << std::endl;
            } else {
                std::cout << "Number of correctable accelerator engine resets attempted by the driver = " << rasState.category[ZES_RAS_ERROR_CAT_RESET] << std::endl;
                std::cout << "Number of correctable errors that have occurred in caches = " << rasState.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS] << std::endl;
                std::cout << "Number of correctable programming errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS] << std::endl;
                std::cout << "Number of correctable driver errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS] << std::endl;
                std::cout << "Number of correctable compute errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS] << std::endl;
                std::cout << "Number of correctable non compute errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS] << std::endl;
                std::cout << "Number of correctable display errors that have occurred  = " << rasState.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS] << std::endl;
            }
        }
        if (iamroot) {
            zes_ras_config_t getConfig = {};
            zes_ras_config_t setConfig = {};
            setConfig.totalThreshold = 14;
            memset(setConfig.detailedThresholds.category, 0, sizeof(setConfig.detailedThresholds.category));
            VALIDATECALL(zesRasSetConfig(handle, &setConfig));
            if (verbose) {
                std::cout << "Setting Total threshold = " << setConfig.totalThreshold << std::endl;
                std::cout << "Setting Threshold for Engine Resets = " << setConfig.detailedThresholds.category[0] << std::endl;
                std::cout << "Setting Threshold for Programming Errors = " << setConfig.detailedThresholds.category[1] << std::endl;
                std::cout << "Setting Threshold for Driver Errors = " << setConfig.detailedThresholds.category[2] << std::endl;
                std::cout << "Setting Threshold for Compute Errors = " << setConfig.detailedThresholds.category[3] << std::endl;
                std::cout << "Setting Threshold for Non Compute Errors = " << setConfig.detailedThresholds.category[4] << std::endl;
                std::cout << "Setting Threshold for Cache Errors = " << setConfig.detailedThresholds.category[5] << std::endl;
                std::cout << "Setting Threshold for Display Errors = " << setConfig.detailedThresholds.category[6] << std::endl;
            }
            VALIDATECALL(zesRasGetConfig(handle, &getConfig));
            if (verbose) {
                std::cout << "Getting Total threshold = " << getConfig.totalThreshold << std::endl;
                std::cout << "Getting Threshold for Engine Resets = " << getConfig.detailedThresholds.category[0] << std::endl;
                std::cout << "Getting Threshold for Programming Errors = " << getConfig.detailedThresholds.category[1] << std::endl;
                std::cout << "Getting Threshold for Driver Errors = " << getConfig.detailedThresholds.category[2] << std::endl;
                std::cout << "Getting Threshold for Compute Errors = " << getConfig.detailedThresholds.category[3] << std::endl;
                std::cout << "Getting Threshold for Non Compute Errors = " << getConfig.detailedThresholds.category[4] << std::endl;
                std::cout << "Getting Threshold for Cache Errors = " << getConfig.detailedThresholds.category[5] << std::endl;
                std::cout << "Getting Threshold for Display Errors = " << getConfig.detailedThresholds.category[6] << std::endl;
            }
        }
    }
}
std::string getStandbyType(zes_standby_type_t standbyType) {
    if (standbyType == ZES_STANDBY_TYPE_GLOBAL)
        return "ZES_STANDBY_TYPE_GLOBAL";
    else
        return "NOT SUPPORTED Standby Type ";
}
std::string getStandbyMode(zes_standby_promo_mode_t standbyMode) {
    if (standbyMode == ZES_STANDBY_PROMO_MODE_DEFAULT)
        return "ZES_STANDBY_PROMO_MODE_DEFAULT";
    else if (standbyMode == ZES_STANDBY_PROMO_MODE_NEVER)
        return "ZES_STANDBY_PROMO_MODE_NEVER";
    else
        return "NOT SUPPORTED Standby Type ";
}
void testSysmanStandby(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Standby tests ---- " << std::endl;
    bool iamroot = (geteuid() == 0);

    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumStandbyDomains(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Standby domains" << std::endl;
        return;
    }
    std::vector<zes_standby_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumStandbyDomains(device, &count, handles.data()));
    for (const auto &handle : handles) {
        zes_standby_properties_t standbyProperties = {};
        zes_standby_promo_mode_t standbyMode = ZES_STANDBY_PROMO_MODE_FORCE_UINT32;

        VALIDATECALL(zesStandbyGetProperties(handle, &standbyProperties));
        if (verbose) {
            std::cout << "standbyProperties.type = " << getStandbyType(standbyProperties.type) << std::endl;
            if (standbyProperties.onSubdevice) {
                std::cout << "standbyProperties.subdeviceId = " << standbyProperties.subdeviceId << std::endl;
            }
        }

        VALIDATECALL(zesStandbyGetMode(handle, &standbyMode));
        if (verbose) {
            std::cout << "standbyMode.type = " << getStandbyMode(standbyMode) << std::endl;
        }

        if (iamroot) {
            std::cout << "Setting Standby Mode Default" << std::endl;
            VALIDATECALL(zesStandbySetMode(handle, ZES_STANDBY_PROMO_MODE_DEFAULT));
            std::cout << "Setting Standby Mode Never" << std::endl;
            VALIDATECALL(zesStandbySetMode(handle, ZES_STANDBY_PROMO_MODE_NEVER));
            // Restore the original mode after the test.
            std::cout << "Restore Standby Mode" << std::endl;
            VALIDATECALL(zesStandbyGetMode(handle, &standbyMode));
        } else {
            std::cout << "Not running as Root. Skipping zetSysmanStandbySetMode test." << std::endl;
        }
    }
}
std::string getEngineType(zes_engine_group_t engineGroup) {
    static const std::map<zes_engine_group_t, std::string> mgetEngineType{
        {ZES_ENGINE_GROUP_COMPUTE_SINGLE, "ZES_ENGINE_GROUP_COMPUTE_SINGLE"},
        {ZES_ENGINE_GROUP_RENDER_SINGLE, "ZES_ENGINE_GROUP_RENDER_SINGLE"},
        {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, "ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE"},
        {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, "ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE"},
        {ZES_ENGINE_GROUP_COPY_SINGLE, "ZES_ENGINE_GROUP_COPY_SINGLE"},
        {ZES_ENGINE_GROUP_ALL, "ZES_ENGINE_GROUP_ALL"},
        {ZES_ENGINE_GROUP_COMPUTE_ALL, "ZES_ENGINE_GROUP_COMPUTE_ALL"},
        {ZES_ENGINE_GROUP_COPY_ALL, "ZES_ENGINE_GROUP_COPY_ALL"},
        {ZES_ENGINE_GROUP_RENDER_ALL, "ZES_ENGINE_GROUP_RENDER_ALL"},
        {ZES_ENGINE_GROUP_MEDIA_ALL, "ZES_ENGINE_GROUP_MEDIA_ALL"},
        {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, "ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE"}};
    auto i = mgetEngineType.find(engineGroup);
    if (i == mgetEngineType.end())
        return "No supported engine group type available";
    else
        return mgetEngineType.at(engineGroup);
}

void testSysmanEngine(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Engine tests ---- " << std::endl;
    zes_device_properties_t deviceProperties = {ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    VALIDATECALL(zesDeviceGetProperties(device, &deviceProperties));
    if (verbose) {
        std::cout << "Device UUID: " << std::endl;
        for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
            std::cout << static_cast<uint32_t>(deviceProperties.core.uuid.id[i]) << " ";
        }
        std::cout << std::endl;
    }

    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumEngineGroups(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Engine domains" << std::endl;
        return;
    }
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumEngineGroups(device, &count, handles.data()));
    for (uint32_t i = 0; i < 10; i++) {
        std::cout << "[" << i << "]" << std::endl;
        for (const auto &handle : handles) {
            zes_engine_properties_t engineProperties = {};
            zes_engine_stats_t engineStats = {};

            VALIDATECALL(zesEngineGetProperties(handle, &engineProperties));

            if (verbose) {
                std::cout << "Engine Type = " << getEngineType(engineProperties.type);
                if (engineProperties.onSubdevice) {
                    std::cout << " || Subdevice Id = " << engineProperties.subdeviceId;
                }
            }

            VALIDATECALL(zesEngineGetActivity(handle, &engineStats));
            if (verbose) {
                std::cout << " || Active Time = " << engineStats.activeTime;
                std::cout << " || Timestamp = " << engineStats.timestamp;
            }
            std::cout << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
    }
}

std::string getSchedulerModeName(zes_sched_mode_t mode) {
    static const std::map<zes_sched_mode_t, std::string> mgetSchedulerModeName{
        {ZES_SCHED_MODE_TIMEOUT, "ZES_SCHED_MODE_TIMEOUT"},
        {ZES_SCHED_MODE_TIMESLICE, "ZES_SCHED_MODE_TIMESLICE"},
        {ZES_SCHED_MODE_EXCLUSIVE, "ZES_SCHED_MODE_EXCLUSIVE"},
        {ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG, "ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG"}};
    auto i = mgetSchedulerModeName.find(mode);
    if (i == mgetSchedulerModeName.end())
        return "NOT SUPPORTED MODE SET";
    else
        return mgetSchedulerModeName.at(mode);
}

void testSysmanScheduler(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Scheduler tests ---- " << std::endl;
    bool iamroot = (geteuid() == 0);

    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumSchedulers(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve scheduler domains" << std::endl;
        return;
    }
    std::vector<zes_sched_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumSchedulers(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_sched_properties_t pProperties = {};
        VALIDATECALL(zesSchedulerGetProperties(handle, &pProperties));
        if (verbose) {
            std::cout << "On subdevice = " << static_cast<uint32_t>(pProperties.onSubdevice) << std::endl;
            std::cout << "SubdeviceId = " << static_cast<uint32_t>(pProperties.subdeviceId) << std::endl;
            std::cout << "Can control = " << static_cast<uint32_t>(pProperties.canControl) << std::endl;
            std::cout << "Engines = " << static_cast<uint32_t>(pProperties.engines) << std::endl;
            std::cout << "Supported Mode = " << static_cast<uint32_t>(pProperties.supportedModes) << std::endl;
        }
        zes_sched_mode_t currentMode = {};
        VALIDATECALL(zesSchedulerGetCurrentMode(handle, &currentMode));
        if (verbose) {
            std::cout << "Current Mode = " << getSchedulerModeName(currentMode) << std::endl;
        }
        zes_sched_timeout_properties_t timeoutProperties = {};
        zes_sched_timeslice_properties_t timesliceProperties = {};

        VALIDATECALL(zesSchedulerGetTimeoutModeProperties(handle, false, &timeoutProperties));
        if (verbose) {
            std::cout << "Timeout Mode Watchdog Timeout = " << timeoutProperties.watchdogTimeout << std::endl;
        }

        VALIDATECALL(zesSchedulerGetTimesliceModeProperties(handle, false, &timesliceProperties));
        if (verbose) {
            std::cout << "Timeslice Mode Interval = " << timesliceProperties.interval << std::endl;
            std::cout << "Timeslice Mode Yield Timeout = " << timesliceProperties.yieldTimeout << std::endl;
        }

        ze_bool_t needReload = 0;

        if (iamroot) {
            std::cout << "Setting Scheduler Timeout Mode" << std::endl;
            VALIDATECALL(zesSchedulerSetTimeoutMode(handle, &timeoutProperties, &needReload));
            std::cout << "Setting Scheduler TimeSlice Mode" << std::endl;
            VALIDATECALL(zesSchedulerSetTimesliceMode(handle, &timesliceProperties, &needReload));
            std::cout << "Setting Scheduler Exclusive Mode" << std::endl;
            VALIDATECALL(zesSchedulerSetExclusiveMode(handle, &needReload));
            std::cout << "Restoring Scheduler Mode" << std::endl;
            // Restore the original mode after the test.
            if (currentMode == ZES_SCHED_MODE_TIMEOUT) {
                VALIDATECALL(zesSchedulerSetTimeoutMode(handle, &timeoutProperties, &needReload));
            } else if (currentMode == ZES_SCHED_MODE_TIMESLICE) {
                VALIDATECALL(zesSchedulerSetTimesliceMode(handle, &timesliceProperties, &needReload));
            } else if (currentMode == ZES_SCHED_MODE_EXCLUSIVE) {
                VALIDATECALL(zesSchedulerSetExclusiveMode(handle, &needReload));
            }
        } else {
            std::cout << "Not running as Root. Skipping zetSysmanSchedulerSetTimeoutMode test." << std::endl;
            std::cout << "Not running as Root. Skipping zetSysmanSchedulerSetTimesliceMode test." << std::endl;
            std::cout << "Not running as Root. Skipping zetSysmanSchedulerSetExclusiveMode test." << std::endl;
        }
    }
}
std::string getMemoryType(zes_mem_type_t memType) {
    static const std::map<zes_mem_type_t, std::string> mgetMemoryType{
        {ZES_MEM_TYPE_HBM, "ZES_MEM_TYPE_HBM"},
        {ZES_MEM_TYPE_DDR, "ZES_MEM_TYPE_DDR"},
        {ZES_MEM_TYPE_DDR3, "ZES_MEM_TYPE_DDR3"},
        {ZES_MEM_TYPE_DDR4, "ZES_MEM_TYPE_DDR4"},
        {ZES_MEM_TYPE_DDR5, "ZES_MEM_TYPE_DDR5"},
        {ZES_MEM_TYPE_LPDDR, "ZES_MEM_TYPE_LPDDR"},
        {ZES_MEM_TYPE_LPDDR3, "ZES_MEM_TYPE_LPDDR3"},
        {ZES_MEM_TYPE_LPDDR4, "ZES_MEM_TYPE_LPDDR4"},
        {ZES_MEM_TYPE_LPDDR5, "ZES_MEM_TYPE_LPDDR5"},
        {ZES_MEM_TYPE_GDDR6, "ZES_MEM_TYPE_GDDR6"},
        {ZES_MEM_TYPE_SRAM, "ZES_MEM_TYPE_SRAM"},
        {ZES_MEM_TYPE_L1, "ZES_MEM_TYPE_L1"},
        {ZES_MEM_TYPE_L3, "ZES_MEM_TYPE_L3"},
        {ZES_MEM_TYPE_GRF, "ZES_MEM_TYPE_GRF"},
        {ZES_MEM_TYPE_SLM, "ZES_MEM_TYPE_SLM"}};
    auto i = mgetMemoryType.find(memType);
    if (i == mgetMemoryType.end())
        return "NOT SUPPORTED MEMORY TYPE SET";
    else
        return mgetMemoryType.at(memType);
}

std::string getMemoryHealth(zes_mem_health_t memHealth) {
    static const std::map<zes_mem_health_t, std::string> mgetMemoryHealth{
        {ZES_MEM_HEALTH_UNKNOWN, "ZES_MEM_HEALTH_UNKNOWN"},
        {ZES_MEM_HEALTH_OK, "ZES_MEM_HEALTH_OK"},
        {ZES_MEM_HEALTH_DEGRADED, "ZES_MEM_HEALTH_DEGRADED"},
        {ZES_MEM_HEALTH_CRITICAL, "ZES_MEM_HEALTH_CRITICAL"},
        {ZES_MEM_HEALTH_REPLACE, "ZES_MEM_HEALTH_REPLACE"}};
    auto i = mgetMemoryHealth.find(memHealth);
    if (i == mgetMemoryHealth.end())
        return "NOT SUPPORTED MEMORY HEALTH SET";
    else
        return mgetMemoryHealth.at(memHealth);
}

std::string getMemoryLocation(zes_mem_loc_t memLocation) {
    if (memLocation == ZES_MEM_LOC_DEVICE) {
        return "ZES_MEM_LOC_DEVICE";
    } else if (memLocation == ZES_MEM_LOC_SYSTEM) {
        return "ZES_MEM_LOC_SYSTEM";
    } else {
        return "NOT SUPPORTED MEMORY LOCATION SET";
    }
}

void testSysmanMemory(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Memory tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumMemoryModules(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Memory domains" << std::endl;
        return;
    }
    std::vector<zes_mem_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumMemoryModules(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_mem_properties_t memoryProperties = {};
        zes_mem_state_t memoryState = {};
        zes_mem_bandwidth_t memoryBandwidth = {};

        VALIDATECALL(zesMemoryGetProperties(handle, &memoryProperties));
        if (verbose) {
            std::cout << "Memory Type = " << getMemoryType(memoryProperties.type) << std::endl;
            std::cout << "On Subdevice = " << static_cast<uint32_t>(memoryProperties.onSubdevice) << std::endl;
            std::cout << "Subdevice Id = " << memoryProperties.subdeviceId << std::endl;
            std::cout << "Memory Location = " << getMemoryLocation(memoryProperties.location) << std::endl;
            std::cout << "Memory Size = " << memoryProperties.physicalSize << std::endl;
            std::cout << "Number of channels = " << memoryProperties.numChannels << std::endl;
            std::cout << "Memory busWidth = " << memoryProperties.busWidth << std::endl;
        }

        VALIDATECALL(zesMemoryGetState(handle, &memoryState));
        if (verbose) {
            std::cout << "Memory Health = " << getMemoryHealth(memoryState.health) << std::endl;
            std::cout << "The total allocatable memory in bytes = " << memoryState.size << std::endl;
            std::cout << "The free memory in bytes = " << memoryState.free << std::endl;
        }

        VALIDATECALL(zesMemoryGetBandwidth(handle, &memoryBandwidth));
        if (verbose) {
            std::cout << "Memory Read Counter = " << memoryBandwidth.readCounter << std::endl;
            std::cout << "Memory Write Counter = " << memoryBandwidth.writeCounter << std::endl;
            std::cout << "Memory Maximum Bandwidth = " << memoryBandwidth.maxBandwidth << std::endl;
            std::cout << "Memory Timestamp = " << memoryBandwidth.timestamp << std::endl;
        }
    }
}

static void trackFirmwareFlashProgress(FirmwareFlashInfo *flashData) {
    bool loopContinue = true;
    uint32_t progressPercent = 0;
    do {
        progressPercent = 0;
        ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
        printf("\rFirmware Flash Progress: %d %%", progressPercent);
        fflush(stdout);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        flashData->firmwareProgressMutex.lock();
        loopContinue = !flashData->flashComplete;
        flashData->firmwareProgressMutex.unlock();

    } while (loopContinue);
    progressPercent = 0;
    ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
    if (result != ZE_RESULT_SUCCESS) {
        return;
    }
    printf("\rFirmware Flash Progress: %d %%\n", progressPercent);
    fflush(stdout);
}

void testSysmanFirmware(ze_device_handle_t &device, std::string imagePath) {
    std::cout << std::endl
              << " ----  firmware tests ---- " << std::endl;
    uint32_t count = 0;
    std::ifstream imageFile;
    uint64_t imgSize = 0;
    FirmwareFlashInfo flashData{};
    if (imagePath.size() != 0) {
        struct stat statBuf;
        auto status = stat(imagePath.c_str(), &statBuf);
        if (!status) {
            imageFile.open(imagePath.c_str(), std::ios::binary);
            imgSize = statBuf.st_size;
        }
    }
    VALIDATECALL(zesDeviceEnumFirmwares(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Firmware domains" << std::endl;
        return;
    }
    std::vector<zes_firmware_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumFirmwares(device, &count, handles.data()));

    for (auto handle : handles) {
        zes_firmware_properties_t fwProperties = {};

        VALIDATECALL(zesFirmwareGetProperties(handle, &fwProperties));
        if (verbose) {
            std::cout << "firmware name = " << fwProperties.name << std::endl;
            std::cout << "On Subdevice = " << static_cast<uint32_t>(fwProperties.onSubdevice) << std::endl;
            std::cout << "Subdevice Id = " << fwProperties.subdeviceId << std::endl;
            std::cout << "firmware version = " << fwProperties.version << std::endl;
        }
        if (imagePath.size() != 0 && imgSize > 0) {
            std::vector<char> img(imgSize);
            imageFile.seekg(0, std::ios::beg);
            imageFile.read(img.data(), imgSize);

            flashData.flashComplete = false;
            flashData.firmwareHandle = handle;
            std::thread thread(trackFirmwareFlashProgress, &flashData);

            VALIDATECALL(zesFirmwareFlash(handle, img.data(), static_cast<uint32_t>(imgSize)));

            flashData.firmwareProgressMutex.lock();
            flashData.flashComplete = true;
            flashData.firmwareProgressMutex.unlock();
            thread.join();

            VALIDATECALL(zesFirmwareGetProperties(handle, &fwProperties));
            if (verbose) {
                std::cout << "firmware name = " << fwProperties.name << std::endl;
                std::cout << "On Subdevice = " << static_cast<uint32_t>(fwProperties.onSubdevice) << std::endl;
                std::cout << "Subdevice Id = " << fwProperties.subdeviceId << std::endl;
                std::cout << "firmware version = " << fwProperties.version << std::endl;
            }
        }
    }
}

void testSysmanReset(ze_device_handle_t &device, bool force) {
    std::cout << std::endl
              << " ----  Reset test (force = " << (force ? "true" : "false") << ") ---- " << std::endl;
    VALIDATECALL(zesDeviceReset(device, force));
}

void testSysmanListenEvents(ze_driver_handle_t driver, std::vector<ze_device_handle_t> &devices, zes_event_type_flags_t events) {
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pEvents = new zes_event_type_flags_t[devices.size()];
    uint32_t timeout = 100000u;
    uint32_t numDevices = static_cast<uint32_t>(devices.size());
    VALIDATECALL(zesDriverEventListen(driver, timeout, numDevices, devices.data(), &numDeviceEvents, pEvents));
    if (verbose) {
        if (numDeviceEvents) {
            for (auto index = 0u; index < devices.size(); index++) {
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                    std::cout << "Device " << index << " got reset required event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                    std::cout << "Device " << index << " got DEVICE_DETACH event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                    std::cout << "Device " << index << " got DEVICE_ATTACH event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS) {
                    std::cout << "Device " << index << " got RAS UNCORRECTABLE event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) {
                    std::cout << "Device " << index << " got RAS CORRECTABLE event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH) {
                    std::cout << "Device " << index << " got Fabric Health event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
                    std::cout << "Device " << index << " got memory Health event" << std::endl;
                }
            }
        }
    }
}

void testSysmanListenEventsEx(ze_driver_handle_t driver, std::vector<ze_device_handle_t> &devices, zes_event_type_flags_t events) {
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pEvents = new zes_event_type_flags_t[devices.size()];
    uint64_t timeout = 10000u;
    uint32_t numDevices = static_cast<uint32_t>(devices.size());
    VALIDATECALL(zesDriverEventListenEx(driver, timeout, numDevices, devices.data(), &numDeviceEvents, pEvents));
    if (verbose) {
        if (numDeviceEvents) {
            for (auto index = 0u; index < devices.size(); index++) {
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                    std::cout << "Device " << index << " got reset required event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                    std::cout << "Device " << index << " got DEVICE_DETACH event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                    std::cout << "Device " << index << " got DEVICE_ATTACH event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS) {
                    std::cout << "Device " << index << " got RAS UNCORRECTABLE event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) {
                    std::cout << "Device " << index << " got RAS CORRECTABLE event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH) {
                    std::cout << "Device " << index << " got Fabric Health event" << std::endl;
                }
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
                    std::cout << "Device " << index << " got memory Health event" << std::endl;
                }
            }
        }
    }
}

std::string getFabricPortStatus(zes_fabric_port_status_t status) {
    static const std::map<zes_fabric_port_status_t, std::string> fabricPortStatus{
        {ZES_FABRIC_PORT_STATUS_UNKNOWN, "ZES_FABRIC_PORT_STATUS_UNKNOWN"},
        {ZES_FABRIC_PORT_STATUS_HEALTHY, "ZES_FABRIC_PORT_STATUS_HEALTHY"},
        {ZES_FABRIC_PORT_STATUS_DEGRADED, "ZES_FABRIC_PORT_STATUS_DEGRADED"},
        {ZES_FABRIC_PORT_STATUS_FAILED, "ZES_FABRIC_PORT_STATUS_FAILED"},
        {ZES_FABRIC_PORT_STATUS_DISABLED, "ZES_FABRIC_PORT_STATUS_DISABLED"}};
    auto i = fabricPortStatus.find(status);
    if (i == fabricPortStatus.end())
        return "UNEXPECTED STATUS";
    else
        return fabricPortStatus.at(status);
}

std::string getFabricPortQualityIssues(zes_fabric_port_qual_issue_flags_t qualityIssues) {
    std::string returnValue;
    returnValue.clear();
    if (qualityIssues & ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS) {
        returnValue.append("ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS ");
    }
    if (qualityIssues & ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED) {
        returnValue.append("ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED");
    }
    return returnValue;
}

std::string getFabricPortFailureReasons(zes_fabric_port_failure_flags_t failureReasons) {
    std::string returnValue;
    returnValue.clear();
    if (failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FAILED) {
        returnValue.append("ZES_FABRIC_PORT_FAILURE_FLAG_FAILED ");
    }
    if (failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT) {
        returnValue.append("ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT ");
    }
    if (failureReasons & ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING) {
        returnValue.append("ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING ");
    }
    return returnValue;
}

void testSysmanFabricPort(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  FabricPort tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumFabricPorts(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve FabricPorts" << std::endl;
        return;
    }
    std::vector<zes_fabric_port_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumFabricPorts(device, &count, handles.data()));

    for (auto handle : handles) {
        zes_fabric_port_properties_t fabricPortProperties = {};
        zes_fabric_link_type_t fabricPortLinkType = {};
        zes_fabric_port_config_t fabricPortConfig = {};
        zes_fabric_port_state_t fabricPortState = {};
        zes_fabric_port_throughput_t fabricPortThroughput = {};

        VALIDATECALL(zesFabricPortGetProperties(handle, &fabricPortProperties));
        if (verbose) {
            std::cout << "Model = \"" << fabricPortProperties.model << "\"" << std::endl;
            std::cout << "On Subdevice = " << static_cast<uint32_t>(fabricPortProperties.onSubdevice) << std::endl;
            std::cout << "Subdevice Id = " << fabricPortProperties.subdeviceId << std::endl;
            std::cout << "Port ID = [" << fabricPortProperties.portId.fabricId
                      << ":" << fabricPortProperties.portId.attachId
                      << ":" << static_cast<uint32_t>(fabricPortProperties.portId.portNumber) << "]" << std::endl;
            std::cout << "Max Rx Speed = " << fabricPortProperties.maxRxSpeed.bitRate
                      << " pbs, " << fabricPortProperties.maxRxSpeed.width << " lanes" << std::endl;
            std::cout << "Max Tx Speed = " << fabricPortProperties.maxTxSpeed.bitRate
                      << " pbs, " << fabricPortProperties.maxTxSpeed.width << " lanes" << std::endl;
        }

        VALIDATECALL(zesFabricPortGetLinkType(handle, &fabricPortLinkType));
        if (verbose) {
            std::cout << "Link Type = \"" << fabricPortLinkType.desc << "\"" << std::endl;
        }

        VALIDATECALL(zesFabricPortGetConfig(handle, &fabricPortConfig));
        if (verbose) {
            std::cout << "Enabled = " << static_cast<uint32_t>(fabricPortConfig.enabled) << std::endl;
            std::cout << "Beaconing = " << static_cast<uint32_t>(fabricPortConfig.beaconing) << std::endl;
        }

        VALIDATECALL(zesFabricPortGetState(handle, &fabricPortState));
        if (verbose) {
            std::cout << "Status = " << getFabricPortStatus(fabricPortState.status) << std::endl;
            std::cout << "Quality Issues = " << getFabricPortQualityIssues(fabricPortState.qualityIssues)
                      << std::hex << fabricPortState.qualityIssues << std::endl;
            std::cout << "Failure Reasons = " << getFabricPortFailureReasons(fabricPortState.failureReasons)
                      << std::hex << fabricPortState.failureReasons << std::endl;
            std::cout << "Remote Port ID = [" << fabricPortState.remotePortId.fabricId
                      << ":" << fabricPortState.remotePortId.attachId
                      << ":" << static_cast<uint32_t>(fabricPortState.remotePortId.portNumber) << "]" << std::endl;
            std::cout << "Rx Speed = " << fabricPortState.rxSpeed.bitRate
                      << " pbs, " << fabricPortState.rxSpeed.width << " lanes" << std::endl;
            std::cout << "Tx Speed = " << fabricPortState.txSpeed.bitRate
                      << " pbs, " << fabricPortState.txSpeed.width << " lanes" << std::endl;
        }

        VALIDATECALL(zesFabricPortGetThroughput(handle, &fabricPortThroughput));
        if (verbose) {
            std::cout << "Timestamp = " << fabricPortThroughput.timestamp << std::endl;
            std::cout << "RX Counter = " << fabricPortThroughput.rxCounter << std::endl;
            std::cout << "TX Counter = " << fabricPortThroughput.txCounter << std::endl;
        }
    }
}

void testSysmanGlobalOperations(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Global Operations tests ---- " << std::endl;
    zes_device_properties_t properties = {};
    VALIDATECALL(zesDeviceGetProperties(device, &properties));
    if (verbose) {
        std::cout << "properties.numSubdevices = " << properties.numSubdevices << std::endl;
        std::cout << "properties.serialNumber = " << properties.serialNumber << std::endl;
        std::cout << "properties.boardNumber = " << properties.boardNumber << std::endl;
        std::cout << "properties.brandName = " << properties.brandName << std::endl;
        std::cout << "properties.modelName = " << properties.modelName << std::endl;
        std::cout << "properties.vendorName = " << properties.vendorName << std::endl;
        std::cout << "properties.driverVersion = " << properties.driverVersion << std::endl;
        std::cout << "properties.core.type = " << properties.core.type << std::endl;
        std::cout << "properties.core.vendorId = " << properties.core.vendorId << std::endl;
        std::cout << "properties.core.deviceId = " << properties.core.deviceId << std::endl;
        std::cout << "properties.core.flags = " << properties.core.flags << std::endl;
        std::cout << "properties.core.coreClockRate = " << properties.core.coreClockRate << std::endl;
        std::cout << "properties.core.maxHardwareContexts = " << properties.core.maxHardwareContexts << std::endl;
        std::cout << "properties.core.maxCommandQueuePriority = " << properties.core.maxCommandQueuePriority << std::endl;
        std::cout << "properties.core.numThreadsPerEU = " << properties.core.numThreadsPerEU << std::endl;
        std::cout << "properties.core.numEUsPerSubslice = " << properties.core.numEUsPerSubslice << std::endl;
        std::cout << "properties.core.numSubslicesPerSlice = " << properties.core.numSubslicesPerSlice << std::endl;
        std::cout << "properties.core.numSlices = " << properties.core.numSlices << std::endl;
        std::cout << "properties.core.timerResolution = " << properties.core.timerResolution << std::endl;
        std::cout << "properties.core.timestampValidBits = " << properties.core.timestampValidBits << std::endl;
        std::cout << "properties.core.kernelTimestampValidBits = " << properties.core.kernelTimestampValidBits << std::endl;
        std::cout << "properties.core.uuid = " << std::endl;
        for (uint32_t i = 0; i < ZE_MAX_UUID_SIZE; i++) {
            std::cout << +properties.core.uuid.id[i] << " ";
        }
        std::cout << std::endl;
        std::cout << "properties.core.name = " << properties.core.name << std::endl;
    }

    uint32_t count = 0;
    VALIDATECALL(zesDeviceProcessesGetState(device, &count, nullptr));
    std::vector<zes_process_state_t> processes(count);
    VALIDATECALL(zesDeviceProcessesGetState(device, &count, processes.data()));
    if (verbose) {
        for (const auto &process : processes) {
            std::cout << "processes.processId = " << process.processId << std::endl;
            std::cout << "processes.memSize = " << process.memSize << std::endl;
            std::cout << "processes.sharedSize = " << process.sharedSize << std::endl;
            std::cout << "processes.engines = " << process.engines << std::endl;
        }
    }
    zes_device_state_t deviceState = {};
    VALIDATECALL(zesDeviceGetState(device, &deviceState));
    if (verbose) {
        std::cout << "reset status: " << deviceState.reset << std::endl;
        std::cout << "repair" << deviceState.repaired << std::endl;
        if (deviceState.reset & ZES_RESET_REASON_FLAG_WEDGED) {
            std::cout << "state reset wedged = " << deviceState.reset << std::endl;
        }
        if (deviceState.reset & ZES_RESET_REASON_FLAG_REPAIR) {
            std::cout << "state reset repair = " << deviceState.reset << std::endl;
            std::cout << "repair state = " << deviceState.repaired << std::endl;
        }
    }
}

void testSysmanDiagnostics(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  diagnostics tests ---- " << std::endl;
    uint32_t count = 0;
    uint32_t subTestCount = 0;
    zes_diag_test_t tests = {};
    zes_diag_result_t results;
    uint32_t start = 0, end = 0;
    VALIDATECALL(zesDeviceEnumDiagnosticTestSuites(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve diagnostics domains" << std::endl;
        return;
    }
    std::vector<zes_diag_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumDiagnosticTestSuites(device, &count, handles.data()));

    for (auto handle : handles) {
        zes_diag_properties_t diagProperties = {};

        VALIDATECALL(zesDiagnosticsGetProperties(handle, &diagProperties));
        if (verbose) {
            std::cout << "diagnostics name = " << diagProperties.name << std::endl;
            std::cout << "On Subdevice = " << diagProperties.onSubdevice << std::endl;
            std::cout << "Subdevice Id = " << diagProperties.subdeviceId << std::endl;
            std::cout << "diagnostics have sub tests = " << diagProperties.haveTests << std::endl;
        }
        if (diagProperties.haveTests != 0) {
            VALIDATECALL(zesDiagnosticsGetTests(handle, &subTestCount, &tests));
            if (verbose) {
                std::cout << "diagnostics subTestCount = " << subTestCount << "for " << diagProperties.name << std::endl;
                for (uint32_t i = 0; i < subTestCount; i++) {
                    std::cout << "subTest#" << tests.index << " = " << tests.name << std::endl;
                }
            }
            end = subTestCount - 1;
        }
        VALIDATECALL(zesDiagnosticsRunTests(handle, start, end, &results));
        if (verbose) {
            switch (results) {
            case ZES_DIAG_RESULT_NO_ERRORS:
                std::cout << "No errors have occurred" << std::endl;
                break;
            case ZES_DIAG_RESULT_REBOOT_FOR_REPAIR:
                std::cout << "diagnostics successful and repair applied, reboot needed" << std::endl;
                break;
            case ZES_DIAG_RESULT_FAIL_CANT_REPAIR:
                std::cout << "diagnostics run, unable to fix" << std::endl;
                break;
            case ZES_DIAG_RESULT_ABORT:
                std::cout << "diagnostics run failed, unknown error" << std::endl;
                break;
            case ZES_DIAG_RESULT_FORCE_UINT32:
            default:
                std::cout << "undefined error" << std::endl;
            }
        }
    }
}

std::string getFanModes(uint32_t fanMode) {
    static const std::map<uint32_t, std::string> mgetFanMode{
        {0, "ZES_FAN_SPEED_MODE_DEFAULT"},
        {1, "ZES_FAN_SPEED_MODE_FIXED"},
        {2, "ZES_FAN_SPEED_MODE_TABLE"}};
    auto i = mgetFanMode.find(fanMode);
    if (i == mgetFanMode.end())
        return "NOT SUPPORTED FAN MODE SET";
    else
        return mgetFanMode.at(fanMode);
}

std::string getFanUnits(uint32_t fanUnit) {
    static const std::map<uint32_t, std::string> mgetFanUnit{
        {0, "ZES_FAN_SPEED_UNITS_RPM"},
        {1, "ZES_FAN_SPEED_UNITS_PERCENT"}};
    auto i = mgetFanUnit.find(fanUnit);
    if (i == mgetFanUnit.end())
        return "NOT SUPPORTED FAN UNIT SET";
    else
        return mgetFanUnit.at(fanUnit);
}

void testSysmanFan(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Fan tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumFans(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Fans" << std::endl;
        return;
    }
    std::vector<zes_fan_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumFans(device, &count, handles.data()));

    for (auto handle : handles) {
        zes_fan_properties_t fanProperties = {};
        zes_fan_config_t fanConfig = {};
        zes_fan_speed_units_t fanUnit = {};
        int32_t fanSpeed;

        VALIDATECALL(zesFanGetProperties(handle, &fanProperties));
        if (verbose) {
            std::cout << "On Subdevice = " << static_cast<uint32_t>(fanProperties.onSubdevice) << std::endl;
            std::cout << "Subdevice Id = " << fanProperties.subdeviceId << std::endl;
            std::cout << "Can control = " << static_cast<uint32_t>(fanProperties.canControl) << std::endl;
            std::cout << "Supported modes = " << getFanModes(fanProperties.supportedModes) << std::endl;
            std::cout << "Supported units = " << getFanUnits(fanProperties.supportedUnits) << std::endl;
            std::cout << "Max RPM = " << fanProperties.maxRPM << std::endl;
            std::cout << "MAX Points = " << fanProperties.maxPoints << std::endl;
        }

        VALIDATECALL(zesFanGetConfig(handle, &fanConfig));
        if (verbose) {
            std::cout << std::endl
                      << " ----  Fan get config tests ---- " << std::endl;
            std::cout << "Mode = " << getFanModes(fanConfig.mode) << std::endl;
            std::cout << "Fan Speed = " << fanConfig.speedFixed.speed << std::endl;
            std::cout << "Fan Speed Unit = " << getFanUnits(fanConfig.speedFixed.units) << std::endl;
        }

        VALIDATECALL(zesFanGetState(handle, fanUnit, &fanSpeed));
        if (verbose) {
            std::cout << std::endl
                      << " ----  Fan get State tests ---- " << std::endl;
            std::cout << "Fan Speed = " << fanSpeed << std::endl;
            std::cout << "Fan Speed Unit = " << getFanUnits(fanUnit) << std::endl;
        }
    }
}

static std::string getMemoryModuleLocation(zes_mem_loc_t type) {
    static const std::map<zes_mem_loc_t, std::string> memoryModuleLocationEnumToStringMap{
        {ZES_MEM_LOC_SYSTEM, "ZES_MEM_LOC_SYSTEM"},
        {ZES_MEM_LOC_DEVICE, "ZES_MEM_LOC_DEVICE"}};
    if (memoryModuleLocationEnumToStringMap.find(type) != memoryModuleLocationEnumToStringMap.end()) {
        return memoryModuleLocationEnumToStringMap.at(type);
    } else {
        return "NOT SUPPORTED MEMORY MODULE LOCATION";
    }
}

void testSysmanVfTelemetry(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  VF Telemetry tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumEnabledVFExp(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Active VF handles" << std::endl;
        return;
    }
    std::cout << "Enabled VF Handle count = " << count << std::endl;
    std::vector<zes_vf_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumEnabledVFExp(device, &count, handles.data()));

    for (const auto &handle : handles) {
        zes_vf_exp2_capabilities_t props = {};
        VALIDATECALL(zesVFManagementGetVFCapabilitiesExp2(handle, &props));
        if (verbose) {
            std::cout << std::endl
                      << "----- PCI BDF ------ " << std::endl;
            std::cout << "Domain: Bus: Device: Function = " << props.address.domain << " : " << props.address.bus << " : " << props.address.device << " : " << props.address.function << std::endl;
            std::cout << "Memory Size in Bytes = " << props.vfDeviceMemSize << std::endl;
            std::cout << "VF Id = " << props.vfID << std::endl;
        }

        // Get Mem utilization
        std::cout << std::endl
                  << "----- Memory Activity Stats ----- " << std::endl;
        count = 0;
        VALIDATECALL(zesVFManagementGetVFMemoryUtilizationExp2(handle, &count, nullptr));
        std::vector<zes_vf_util_mem_exp2_t> memUtils(count);
        VALIDATECALL(zesVFManagementGetVFMemoryUtilizationExp2(handle, &count, memUtils.data()));
        for (uint32_t it = 0; it < count; it++) {
            if (verbose) {
                std::cout << "Location of the Memory = " << getMemoryModuleLocation(memUtils[it].vfMemLocation) << std::endl;
                std::cout << "Memory Utilized in Bytes = " << memUtils[it].vfMemUtilized << std::endl;
            }
        }

        std::cout << std::endl
                  << "----- Engine Activity Stats ----- " << std::endl;
        count = 0;
        VALIDATECALL(zesVFManagementGetVFEngineUtilizationExp2(handle, &count, nullptr));
        std::vector<zes_vf_util_engine_exp2_t> engineUtils(count);
        VALIDATECALL(zesVFManagementGetVFEngineUtilizationExp2(handle, &count, engineUtils.data()));
        for (uint32_t it = 0; it < count; it++) {
            if (verbose) {
                std::cout << "Engine Type = " << getEngineType(engineUtils[it].vfEngineType) << std::endl;
                std::cout << "Active Counter Value = " << engineUtils[it].activeCounterValue << std::endl;
                std::cout << "Sampling Counter Value = " << engineUtils[it].samplingCounterValue << std::endl;
            }
        }
    }
}

bool checkpFactorArguments(std::vector<ze_device_handle_t> &devices, std::vector<std::string> &buf) {
    uint32_t deviceIndex = static_cast<uint32_t>(std::stoi(buf[1]));
    if (deviceIndex >= devices.size()) {
        return false;
    }
    uint32_t subDeviceCount = 0;
    zes_device_properties_t properties = {};
    VALIDATECALL(zesDeviceGetProperties(devices[deviceIndex], &properties));
    subDeviceCount = properties.numSubdevices;
    uint32_t subDeviceIndex = static_cast<uint32_t>(std::stoi(buf[2]));
    if (subDeviceCount > 0 && subDeviceIndex >= subDeviceCount) {
        return false;
    }
    zes_engine_type_flags_t engineTypeFlag = getEngineFlagType(buf[3]);
    if (engineTypeFlag == ZES_ENGINE_TYPE_FLAG_FORCE_UINT32) {
        return false;
    }
    return true;
}

bool validatePowerLimitArguments(const size_t devCount, std::vector<std::string> &buf) {
    if ((buf.size() != 4 || buf[0] != "--setlimit" || (!(buf[1] == "--sustained" || buf[1] == "--peak" || buf[1] == "--instantaneous" || buf[1] == "--burst")))) {
        return false;
    }

    uint32_t devIndex = static_cast<uint32_t>(std::stoi(buf[2]));
    if (devIndex >= devCount) {
        return false;
    }

    return true;
}

bool validateGetenv(const char *name) {
    const char *env = getenv(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}
int enableSysman() {
    int ret = 0;
#if defined(_WIN32) || defined(_WIN64)
    ret = _putenv_s("ZES_ENABLE_SYSMAN", "1");
#else  // defined(_WIN32) || defined(_WIN64)
    ret = setenv("ZES_ENABLE_SYSMAN", "1", 1);
#endif // defined(_WIN32) || defined(_WIN64)
    return ret;
}

int main(int argc, char *argv[]) {

    std::vector<ze_device_handle_t> devices;
    ze_driver_handle_t driver;

    if (validateGetenv("ZELLO_SYSMAN_USE_ZESINIT")) {
        if (validateGetenv("ZES_ENABLE_SYSMAN")) {
            std::cout << "ZES_ENABLE_SYSMAN environment variable Set" << std::endl;
        }

        else {
            std::cout << "ZES_ENABLE_SYSMAN environment variable Not Set" << std::endl;
        }
        getSysmanDeviceHandles(driver, devices);
        std::cout << "Sysman Initialization done via zesInit" << std::endl;
    }

    else {
        if (validateGetenv("ZES_ENABLE_SYSMAN")) {
            std::cout << "ZES_ENABLE_SYSMAN environment variable Set" << std::endl;
        }

        else {
            std::cout << "ZES_ENABLE_SYSMAN environment variable Not Set" << std::endl;
            std::cout << "Setting the environment variable ZES_ENABLE_SYSMAN " << std::endl;
            if (enableSysman()) {
                return 0;
            }
            std::cout << "ZES_ENABLE_SYSMAN environment variable Set" << std::endl;
        }
        getDeviceHandles(driver, devices);
        std::cout << "Sysman Initialization done via zeInit" << std::endl;
    }

    bool force = false;
    bool pFactorIsSet = true;
    std::vector<std::string> buf;
    uint32_t deviceIndex = 0;
    int optind = 0;
    char *optarg = nullptr;

    if (isParamEnabled(argc, argv, "-h", "--help", &optind)) {
        usage();
        exit(0);
    }
    if (isParamEnabled(argc, argv, "-p", "--pci", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanPci(device);
        });
    }
    if (isParamEnabled(argc, argv, "-z", "--survive", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanSurvivability(device);
        });
    }
    if (isParamEnabled(argc, argv, "-P", "--performance", &optind)) {
        deviceIndex = 0;
        optind = optind + 1;
        while (optind < argc) {
            buf.push_back(argv[optind]);
            optind++;
        }
        if (buf.size() != 0 && (buf.size() != 5 || buf[0] != "--setconfig")) {
            usage();
            exit(0);
        }
        if (buf.size() != 0) {
            if (checkpFactorArguments(devices, buf) == false) {
                std::cout << "Invalid arguments passed for setting performance factor" << std::endl;
                usage();
                exit(0);
            }
            pFactorIsSet = false;
        }
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanPerformance(device, buf, deviceIndex, pFactorIsSet);
        });
        if (pFactorIsSet == false) {
            std::cout << "Unable to set the Performance factor" << std::endl;
        }
        buf.clear();
    }
    if (isParamEnabled(argc, argv, "-f", "--frequency", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanFrequency(device);
        });
    }
    if (isParamEnabled(argc, argv, "-s", "--standby", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanStandby(device);
        });
    }
    if (isParamEnabled(argc, argv, "-e", "--engine", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanEngine(device);
        });
    }
    if (isParamEnabled(argc, argv, "-c", "--scheduler", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanScheduler(device);
        });
    }
    if (isParamEnabled(argc, argv, "-t", "--temperature", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanTemperature(device);
        });
    }
    if (isParamEnabled(argc, argv, "-C", "--ecc", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanEcc(device);
        });
    }
    if (isParamEnabled(argc, argv, "-o", "--power", &optind)) {
        deviceIndex = 0;
        optind = optind + 1;
        while (optind < argc) {
            buf.push_back(argv[optind]);
            optind++;
        }

        if (buf.size() != 0) {
            if (validatePowerLimitArguments(devices.size(), buf) == false) {
                std::cout << "Invalid Arguments passed to set power limit" << std::endl;
                usage();
                exit(0);
            }
        }
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanPower(device, buf, deviceIndex);
        });
    }
    if (isParamEnabled(argc, argv, "-g", "--global", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanGlobalOperations(device);
        });
    }
    if (isParamEnabled(argc, argv, "-m", "--memory", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanMemory(device);
        });
    }
    if (isParamEnabled(argc, argv, "-R", "--ras", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanRas(device);
        });
    }
    if (isParamEnabled(argc, argv, "-re", "--rasexp", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanRasExp(device);
        });
    }
    if (isParamEnabled(argc, argv, "-i", "--firmware", &optind)) {
        optind = optind + 1;
        if (optind < argc) {
            optarg = argv[optind];
        }
        std::string filePathFirmware;
        if (optarg != nullptr) {
            filePathFirmware = optarg;
        }
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanFirmware(device, filePathFirmware);
        });
    }
    if (isParamEnabled(argc, argv, "-r", "--reset", &optind)) {
        optind = optind + 1;
        if (optind < argc) {
            optarg = argv[optind];
        }
        if (!strcmp(optarg, "force")) {
            force = true;
        } else if (!strcmp(optarg, "noforce")) {
            force = false;
        } else {
            usage();
            exit(0);
        }
        optind++;
        if (optind < argc) {
            deviceIndex = static_cast<uint32_t>(std::stoi(argv[optind]));
            if (deviceIndex >= devices.size()) {
                std::cout << "Invalid deviceId specified for device reset" << std::endl;
                usage();
                exit(0);
            }
            testSysmanReset(devices[deviceIndex], force);
        } else {
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanReset(device, force);
            });
        }
    }
    if (isParamEnabled(argc, argv, "-E", "--event", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            VALIDATECALL(zesDeviceEventRegister(device,
                                                ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                                    ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                                    ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH | ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
        });
        testSysmanListenEvents(driver, devices,
                               ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                   ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                   ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH);
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            VALIDATECALL(zesDeviceEventRegister(device,
                                                ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                                    ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                                    ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH | ZES_EVENT_TYPE_FLAG_MEM_HEALTH));
        });
        testSysmanListenEventsEx(driver, devices,
                                 ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                     ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                     ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS | ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH);
    }
    if (isParamEnabled(argc, argv, "-F", "--fabricport", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanFabricPort(device);
        });
    }
    if (isParamEnabled(argc, argv, "-d", "--diagnostics", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanDiagnostics(device);
        });
    }
    if (isParamEnabled(argc, argv, "-a", "--fan", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanFan(device);
        });
    }

    if (isParamEnabled(argc, argv, "-v", "--vftelemetry", &optind)) {
        std::for_each(devices.begin(), devices.end(), [&](auto device) {
            testSysmanVfTelemetry(device);
        });
    }

    return 0;
}

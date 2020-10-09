/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/zes_api.h>

#include <algorithm>
#include <getopt.h>
#include <iostream>
#include <map>
#include <string.h>
#include <unistd.h>
#include <vector>

bool verbose = true;

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
    std::cout << "\n set Env variable ZES_ENABLE_SYSMAN=1"
                 "\n"
                 "\n zello_sysman [OPTIONS]"
                 "\n"
                 "\n OPTIONS:"
                 "\n  -p,   --pci                   selectively run pci black box test"
                 "\n  -f,   --frequency             selectively run frequency black box test"
                 "\n  -s,   --standby               selectively run standby black box test"
                 "\n  -e,   --engine                selectively run engine black box test"
                 "\n  -c,   --scheduler             selectively run scheduler black box test"
                 "\n  -t,   --temperature           selectively run temperature black box test"
                 "\n  -o,   --power                 selectively run power black box test"
                 "\n  -m,   --memory                selectively run memory black box test"
                 "\n  -g,   --global                selectively run device/global operations black box test"
                 "\n  -E,   --event                 set and listen to events black box test"
                 "\n  -r,   --reset force|noforce   selectively run device reset test"
                 "\n  -h,   --help                  display help message"
                 "\n"
                 "\n  All L0 Syman APIs that set values require root privileged execution"
                 "\n"
                 "\n";
}

void getDeviceHandles(ze_driver_handle_t &driverHandle, std::vector<ze_device_handle_t> &devices, int argc, char *argv[]) {

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

    ze_device_properties_t deviceProperties = {};
    for (auto device : devices) {
        VALIDATECALL(zeDeviceGetProperties(device, &deviceProperties));

        if (verbose) {
            std::cout << "Device Name = " << deviceProperties.name << std::endl;
        }
    }
}

void testSysmanPower(ze_device_handle_t &device) {
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

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;
        VALIDATECALL(zesPowerGetEnergyCounter(handle, &energyCounter));
        if (verbose) {
            std::cout << "energyCounter.energy = " << energyCounter.energy << std::endl;
            std::cout << "energyCounter.timestamp = " << energyCounter.timestamp << std::endl;
        }
        zes_power_sustained_limit_t sustainedGetDefault = {};
        zes_power_burst_limit_t burstGetDefault = {};
        VALIDATECALL(zesPowerGetLimits(handle, &sustainedGetDefault, &burstGetDefault, nullptr));
        if (verbose) {
            std::cout << "sustainedGetDefault.enabled = " << sustainedGetDefault.enabled << std::endl;
            if (sustainedGetDefault.enabled) {
                std::cout << "sustainedGetDefault.power = " << sustainedGetDefault.power << std::endl;
                std::cout << "sustainedGetDefault.interval = " << sustainedGetDefault.interval << std::endl;
            }
            std::cout << "burstGetDefault.enabled = " << burstGetDefault.enabled << std::endl;
            if (burstGetDefault.enabled) {
                std::cout << "burstGetDefault.power = " << burstGetDefault.power << std::endl;
            }
        }
        if (iamroot) {
            zes_power_sustained_limit_t sustainedSet = {};
            sustainedSet.power = sustainedGetDefault.power - sustainedGetDefault.power / 10; //Randomly try to reduce power
            sustainedSet.interval = sustainedGetDefault.interval - sustainedGetDefault.interval / 10;
            zes_power_burst_limit_t burstSet = {};
            if (burstGetDefault.enabled) {
                burstSet.enabled = 0;
            }
            VALIDATECALL(zesPowerSetLimits(handle, &sustainedSet, &burstSet, nullptr));
            if (verbose) {
                std::cout << "zesPowerSetLimits success" << std::endl;
                std::cout << "Now restore the power values to default ones" << std::endl;
            }
            VALIDATECALL(zesPowerSetLimits(handle, &sustainedGetDefault, &burstGetDefault, nullptr));
        }
    }
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

    for (auto handle : handles) {
        double temperature;
        VALIDATECALL(zesTemperatureGetState(handle, &temperature));
        if (verbose) {
            std::cout << "temperature current state is: " << temperature << std::endl;
        }
    }
}

void testSysmanPci(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  PCI tests ---- " << std::endl;
    zes_pci_properties_t properties = {};
    VALIDATECALL(zesDevicePciGetProperties(device, &properties));
    if (verbose) {
        std::cout << "properties.address.domain = " << properties.address.domain << std::endl;
        std::cout << "properties.address.bus = " << properties.address.bus << std::endl;
        std::cout << "properties.address.device = " << properties.address.device << std::endl;
        std::cout << "properties.maxSpeed.gen = " << properties.maxSpeed.gen << std::endl;
        std::cout << "properties.maxSpeed.width = " << properties.maxSpeed.width << std::endl;
        std::cout << "properties.maxSpeed.maxBandwidth = " << properties.maxSpeed.maxBandwidth << std::endl;
    }

    uint32_t count = 0;
    VALIDATECALL(zesDevicePciGetBars(device, &count, nullptr));
    if (verbose) {
        std::cout << "Bar count = " << count << std::endl;
    }
    std::vector<zes_pci_bar_properties_t> pciBarProps(count);
    VALIDATECALL(zesDevicePciGetBars(device, &count, pciBarProps.data()));
    if (verbose) {
        for (uint32_t i = 0; i < count; i++) {
            std::cout << "pciBarProps.type = " << std::hex << pciBarProps[i].type << std::endl;
            std::cout << "pciBarProps.index = " << std::hex << pciBarProps[i].index << std::endl;
            std::cout << "pciBarProps.base = " << std::hex << pciBarProps[i].base << std::endl;
            std::cout << "pciBarProps.size = " << std::hex << pciBarProps[i].size << std::endl;
        }
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

    for (auto handle : handles) {
        zes_freq_properties_t freqProperties = {};
        zes_freq_range_t freqRange = {};
        zes_freq_range_t testFreqRange = {};
        zes_freq_state_t freqState = {};

        VALIDATECALL(zesFrequencyGetProperties(handle, &freqProperties));
        if (verbose) {
            std::cout << "freqProperties.type = " << freqProperties.type << std::endl;
            std::cout << "freqProperties.canControl = " << freqProperties.canControl << std::endl;
            std::cout << "freqProperties.isThrottleEventSupported = " << freqProperties.isThrottleEventSupported << std::endl;
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
    for (auto handle : handles) {
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
        {ZES_ENGINE_GROUP_COPY_SINGLE, "ZES_ENGINE_GROUP_COPY_SINGLE"}};
    auto i = mgetEngineType.find(engineGroup);
    if (i == mgetEngineType.end())
        return "NOT SUPPORTED MODE Engine avalialbe";
    else
        return mgetEngineType.at(engineGroup);
}

void testSysmanEngine(ze_device_handle_t &device) {
    std::cout << std::endl
              << " ----  Engine tests ---- " << std::endl;
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumEngineGroups(device, &count, nullptr));
    if (count == 0) {
        std::cout << "Could not retrieve Engine domains" << std::endl;
        return;
    }
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    VALIDATECALL(zesDeviceEnumEngineGroups(device, &count, handles.data()));
    for (auto handle : handles) {
        zes_engine_properties_t engineProperties = {};
        zes_engine_stats_t engineStats = {};

        VALIDATECALL(zesEngineGetProperties(handle, &engineProperties));

        if (verbose) {
            std::cout << "Engine Type = " << getEngineType(engineProperties.type) << std::endl;
            if (engineProperties.onSubdevice) {
                std::cout << "Subdevice Id = " << engineProperties.subdeviceId << std::endl;
            }
        }

        VALIDATECALL(zesEngineGetActivity(handle, &engineStats));
        if (verbose) {
            std::cout << "Active Time = " << engineStats.activeTime << std::endl;
            std::cout << "Timestamp = " << engineStats.timestamp << std::endl;
        }
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

    for (auto handle : handles) {
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

    for (auto handle : handles) {
        zes_mem_properties_t memoryProperties = {};
        zes_mem_state_t memoryState = {};
        zes_mem_bandwidth_t memoryBandwidth = {};

        VALIDATECALL(zesMemoryGetProperties(handle, &memoryProperties));
        if (verbose) {
            std::cout << "Memory Type = " << getMemoryType(memoryProperties.type) << std::endl;
            std::cout << "On Subdevice = " << memoryProperties.onSubdevice << std::endl;
            std::cout << "Subdevice Id = " << memoryProperties.subdeviceId << std::endl;
            std::cout << "Memory Size = " << memoryProperties.physicalSize << std::endl;
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

void testSysmanReset(ze_device_handle_t &device, bool force) {
    std::cout << std::endl
              << " ----  Reset test (force = " << (force ? "true" : "false") << ") ---- " << std::endl;
    VALIDATECALL(zesDeviceReset(device, force));
}

void testSysmanListenEvents(ze_driver_handle_t driver, std::vector<ze_device_handle_t> &devices, zes_event_type_flags_t events) {
    uint32_t numDeviceEvents = 0;
    zes_event_type_flags_t *pEvents = new zes_event_type_flags_t[devices.size()];
    uint32_t timeout = 10000u;
    uint32_t numDevices = static_cast<uint32_t>(devices.size());
    VALIDATECALL(zesDriverEventListen(driver, timeout, numDevices, devices.data(), &numDeviceEvents, pEvents));
    if (verbose) {
        if (numDeviceEvents) {
            for (auto index = 0u; index < devices.size(); index++) {
                if (pEvents[index] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                    std::cout << "Device " << index << "got reset required event" << std::endl;
                }
            }
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
        std::cout << "properties.driverVersion= " << properties.driverVersion << std::endl;
    }

    uint32_t count = 0;
    VALIDATECALL(zesDeviceProcessesGetState(device, &count, nullptr));
    std::vector<zes_process_state_t> processes(count);
    VALIDATECALL(zesDeviceProcessesGetState(device, &count, processes.data()));
    if (verbose) {
        for (auto process : processes) {
            std::cout << "processes.processId = " << process.processId << std::endl;
            std::cout << "processes.memSize = " << process.memSize << std::endl;
            std::cout << "processes.sharedSize = " << process.sharedSize << std::endl;
            std::cout << "processes.engines = " << process.engines << std::endl;
        }
    }
}
bool validateGetenv(const char *name) {
    const char *env = getenv(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}
int main(int argc, char *argv[]) {
    std::vector<ze_device_handle_t> devices;
    ze_driver_handle_t driver;

    if (!validateGetenv("ZES_ENABLE_SYSMAN")) {
        std::cout << "Must set environment variable ZES_ENABLE_SYSMAN=1" << std::endl;
        exit(0);
    }
    getDeviceHandles(driver, devices, argc, argv);
    int opt;
    static struct option long_opts[] = {
        {"help", no_argument, nullptr, 'h'},
        {"pci", no_argument, nullptr, 'p'},
        {"frequency", no_argument, nullptr, 'f'},
        {"standby", no_argument, nullptr, 's'},
        {"engine", no_argument, nullptr, 'e'},
        {"scheduler", no_argument, nullptr, 'c'},
        {"temperature", no_argument, nullptr, 't'},
        {"power", no_argument, nullptr, 'o'},
        {"global", no_argument, nullptr, 'g'},
        {"memory", no_argument, nullptr, 'm'},
        {"event", no_argument, nullptr, 'E'},
        {"reset", required_argument, nullptr, 'r'},
        {0, 0, 0, 0},
    };
    bool force = false;
    while ((opt = getopt_long(argc, argv, "hpfsectogmrE:", long_opts, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            usage();
            exit(0);
            break;
        case 'p':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanPci(device);
            });
            break;
        case 'f':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanFrequency(device);
            });
            break;
        case 's':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanStandby(device);
            });
            break;
        case 'e':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanEngine(device);
            });
            break;
        case 'c':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanScheduler(device);
            });
            break;
        case 't':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanTemperature(device);
            });
            break;
        case 'o':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanPower(device);
            });
            break;
        case 'g':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanGlobalOperations(device);
            });
            break;
        case 'm':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanMemory(device);
            });
            break;
        case 'r':
            if (!strcmp(optarg, "force")) {
                force = true;
            } else if (!strcmp(optarg, "noforce")) {
                force = false;
            } else {
                usage();
                exit(0);
            }
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                testSysmanReset(device, force);
            });
            break;
        case 'E':
            std::for_each(devices.begin(), devices.end(), [&](auto device) {
                zesDeviceEventRegister(device, ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED);
            });
            testSysmanListenEvents(driver, devices, ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED);
            break;

        default:
            usage();
            exit(0);
        }
    }

    return 0;
}

/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

static const std::map<__u16, std::string> xeEngineClassToSysfsEngineMap = {
    {DRM_XE_ENGINE_CLASS_RENDER, "rcs"},
    {DRM_XE_ENGINE_CLASS_COMPUTE, "ccs"},
    {DRM_XE_ENGINE_CLASS_COPY, "bcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_DECODE, "vcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

SysmanKmdInterfaceXe::SysmanKmdInterfaceXe(SysmanProductHelper *pSysmanProductHelper) {
    initSysfsNameToFileMap(pSysmanProductHelper);
    initSysfsNameToNativeUnitMap(pSysmanProductHelper);
}

SysmanKmdInterfaceXe::~SysmanKmdInterfaceXe() = default;

std::string SysmanKmdInterfaceXe::getBasePath(uint32_t subDeviceId) const {
    return "device/tile" + std::to_string(subDeviceId) + "/gt" + std::to_string(subDeviceId) + "/";
}

void SysmanKmdInterfaceXe::initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper) {
    sysfsNameToFileMap[SysfsName::sysfsNameMinFrequency] = std::make_pair("freq0/min_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxFrequency] = std::make_pair("freq0/max_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameCurrentFrequency] = std::make_pair("freq0/cur_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameActualFrequency] = std::make_pair("freq0/act_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameEfficientFrequency] = std::make_pair("freq0/rpe_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxValueFrequency] = std::make_pair("freq0/rp0_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinValueFrequency] = std::make_pair("freq0/rpn_freq", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonStatus] = std::make_pair("freq0/throttle/status", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL1] = std::make_pair("freq0/throttle/reason_pl1", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL2] = std::make_pair("freq0/throttle/reason_pl2", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL4] = std::make_pair("freq0/throttle/reason_pl4", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonThermal] = std::make_pair("freq0/throttle/reason_thermal", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = std::make_pair("", "power1_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimitInterval] = std::make_pair("", "power1_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode] = std::make_pair("", "energy2_input");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = std::make_pair("", "power1_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = std::make_pair("", pSysmanProductHelper->getPackageCriticalPowerLimitFile());
    sysfsNameToFileMap[SysfsName::sysfsNameCardEnergyCounterNode] = std::make_pair("", "energy1_input");
    sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange] = std::make_pair("physical_vram_size_bytes", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxMemoryFrequency] = std::make_pair("freq_vram_rp0", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinMemoryFrequency] = std::make_pair("freq_vram_rpn", "");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeout] = std::make_pair("", "preempt_timeout_us");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeslice] = std::make_pair("", "timeslice_duration_us");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = std::make_pair("", "job_timeout_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerWatchDogTimeoutMaximum] = std::make_pair("", "job_timeout_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactor] = std::make_pair("base_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactorScale] = std::make_pair("base_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactor] = std::make_pair("media_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale] = std::make_pair("media_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceSystemPowerBalance] = std::make_pair("", "sys_pwr_balance");
}

void SysmanKmdInterfaceXe::initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper) {
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeout] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeslice] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerWatchDogTimeoutMaximum] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = pSysmanProductHelper->getPackageCriticalPowerLimitNativeUnit();
}

std::string SysmanKmdInterfaceXe::getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool prefixBaseDirectory) {
    if (sysfsNameToFileMap.find(sysfsName) != sysfsNameToFileMap.end()) {
        std::string filePath = prefixBaseDirectory ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
        return filePath;
    }
    // All sysfs accesses are expected to be covered
    DEBUG_BREAK_IF(1);
    return {};
}

std::string SysmanKmdInterfaceXe::getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) {
    std::string filePathPhysicalMemorySize = "device/tile" + std::to_string(subDeviceId) + "/" +
                                             sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange].first;
    return filePathPhysicalMemorySize;
}

std::string SysmanKmdInterfaceXe::getEnergyCounterNodeFile(zes_power_domain_t powerDomain) {
    std::string filePath = {};
    if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        filePath = sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode].second;
    } else if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        filePath = sysfsNameToFileMap[SysfsName::sysfsNameCardEnergyCounterNode].second;
    }
    return filePath;
}

ze_result_t SysmanKmdInterfaceXe::getEngineActivityFdList(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface, std::vector<std::pair<int64_t, int64_t>> &fdList) {

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto engineClass = engineGroupToEngineClass.find(engineGroup);
    if (engineClass == engineGroupToEngineClass.end()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Engine Group not supported and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    const std::string activeTicksEventFile = std::string(sysDevicesDir) + sysmanDeviceDirName + "/events/engine-active-ticks";
    uint64_t activeTicksConfig = UINT64_MAX;
    auto ret = pPmuInterface->getConfigFromEventFile(activeTicksEventFile, activeTicksConfig);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get config for the active ticks from event file and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    const std::string totalTicksEventFile = std::string(sysDevicesDir) + "/" + sysmanDeviceDirName + "/events/engine-total-ticks";
    uint64_t totalTicksConfig = UINT64_MAX;
    ret = pPmuInterface->getConfigFromEventFile(totalTicksEventFile, totalTicksConfig);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get config for the total ticks from event file and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    const std::string formatDir = std::string(sysDevicesDir) + sysmanDeviceDirName + "/format/";
    ret = pPmuInterface->getConfigAfterFormat(formatDir, activeTicksConfig, engineClass->second, engineInstance, subDeviceId);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get config for the active ticks after format and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    ret = pPmuInterface->getConfigAfterFormat(formatDir, totalTicksConfig, engineClass->second, engineInstance, subDeviceId);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get config for the total ticks after format and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    int64_t fd[2];
    fd[0] = pPmuInterface->pmuInterfaceOpen(activeTicksConfig, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
    if (fd[0] < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle \n", __FUNCTION__);
        return checkErrorNumberAndReturnStatus();
    }

    fd[1] = pPmuInterface->pmuInterfaceOpen(totalTicksConfig, static_cast<int>(fd[0]), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
    if (fd[1] < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Total Active Ticks Handle \n", __FUNCTION__);
        close(static_cast<int>(fd[0]));
        return checkErrorNumberAndReturnStatus();
    }

    fdList.push_back(std::make_pair(fd[0], fd[1]));

    return result;
}

ze_result_t SysmanKmdInterfaceXe::readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) {
    uint64_t data[4] = {};

    auto ret = pPmuInterface->pmuRead(static_cast<int>(fdPair.first), data, sizeof(data));
    if (ret < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    pStats->activeTime = data[2];
    pStats->timestamp = data[3] ? data[3] : SysmanDevice::getSysmanTimestamp();
    return ZE_RESULT_SUCCESS;
}

std::string SysmanKmdInterfaceXe::getHwmonName(uint32_t subDeviceId, bool isSubdevice) const {
    return "xe";
}

std::vector<zes_power_domain_t> SysmanKmdInterfaceXe::getPowerDomains() const {
    return {ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_GPU, ZES_POWER_DOMAIN_MEMORY};
}

std::optional<std::string> SysmanKmdInterfaceXe::getEngineClassString(uint16_t engineClass) {
    auto sysfEngineString = xeEngineClassToSysfsEngineMap.find(engineClass);
    if (sysfEngineString == xeEngineClassToSysfsEngineMap.end()) {
        DEBUG_BREAK_IF(true);
        return {};
    }
    return sysfEngineString->second;
}

std::string SysmanKmdInterfaceXe::getEngineBasePath(uint32_t subDeviceId) const {
    return getBasePath(subDeviceId) + "engines";
}

ze_result_t SysmanKmdInterfaceXe::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                               LinuxSysmanImp *pLinuxSysmanImp,
                                                               SysFsAccessInterface *pSysfsAccess,
                                                               ze_bool_t onSubdevice,
                                                               uint32_t subdeviceId) {
    if (onSubdevice) {
        return getNumEngineTypeAndInstancesForSubDevices(mapOfEngines,
                                                         pLinuxSysmanImp->getDrm(), subdeviceId);
    }
    return getNumEngineTypeAndInstancesForDevice(getEngineBasePath(subdeviceId), mapOfEngines, pSysfsAccess);
}

void SysmanKmdInterfaceXe::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {

    auto pFsAccess = getFsAccess();
    const std::string srcVersionFile("/sys/module/xe/srcversion");
    std::string strVal = {};
    ze_result_t result = pFsAccess->read(srcVersionFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read driver version from %s and returning error:0x%x\n", __FUNCTION__, srcVersionFile.c_str(), result);
        std::strncpy(driverVersion, unknown.data(), ZES_STRING_PROPERTY_SIZE);
    } else {
        std::strncpy(driverVersion, strVal.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    return;
}

ze_result_t SysmanKmdInterfaceXe::getBusyAndTotalTicksConfigs(uint64_t fnNumber, uint64_t engineInstance, uint64_t engineClass, std::pair<uint64_t, uint64_t> &configPair) {
    return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
}

std::string SysmanKmdInterfaceXe::getGpuBindEntry() const {
    return "/sys/bus/pci/drivers/xe/bind";
}

std::string SysmanKmdInterfaceXe::getGpuUnBindEntry() const {
    return "/sys/bus/pci/drivers/xe/unbind";
}

void SysmanKmdInterfaceXe::setSysmanDeviceDirName(const bool isIntegratedDevice) {

    sysmanDeviceDirName = "xe";
    getDeviceDirName(sysmanDeviceDirName, isIntegratedDevice);
}

} // namespace Sysman
} // namespace L0

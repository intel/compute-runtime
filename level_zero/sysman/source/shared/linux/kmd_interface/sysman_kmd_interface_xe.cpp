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

/*
 *   Power related Syfs Nodes summary For XE supported platforms
 *     1. CARD
 *         BurstPowerLimit              = "power1_cap"
 *         BurstPowerLimitInterval      = "power1_cap_interval"
 *         SustainedPowerLimit          = "power1_max";
 *         SustainedPowerLimitInterval  = "power1_max_interval";
 *         DefaultPowerLimit            = "power1_rated_max";
 *         EnergyCounterNode            = "energy1_input";
 *         CriticalPowerLimit           = "power1_crit"
 *     2. PACKAGE
 *         BurstPowerLimit              = "power2_cap"
 *         BurstPowerLimitInterval      = "power2_cap_interval"
 *         SustainedPowerLimit          = "power2_max";
 *         SustainedPowerLimitInterval  = "power2_max_interval";
 *         DefaultPowerLimit            = "power2_rated_max";
 *         EnergyCounterNode            = "energy2_input";
 *         CriticalPowerLimit           = "power2_crit"
 */

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
    sysfsNameToFileMap[SysfsName::sysfsNameCardEnergyCounterNode] = std::make_pair("", "energy1_input");
    sysfsNameToFileMap[SysfsName::sysfsNameCardBurstPowerLimit] = std::make_pair("", "power1_cap");
    sysfsNameToFileMap[SysfsName::sysfsNameCardBurstPowerLimitInterval] = std::make_pair("", "power1_cap_interval");
    sysfsNameToFileMap[SysfsName::sysfsNameCardSustainedPowerLimit] = std::make_pair("", "power1_max");
    sysfsNameToFileMap[SysfsName::sysfsNameCardSustainedPowerLimitInterval] = std::make_pair("", "power1_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNameCardDefaultPowerLimit] = std::make_pair("", "power1_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNameCardCriticalPowerLimit] = std::make_pair("", "power1_crit");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode] = std::make_pair("", "energy2_input");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageBurstPowerLimit] = std::make_pair("", "power2_cap");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageBurstPowerLimitInterval] = std::make_pair("", "power2_cap_interval");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = std::make_pair("", "power2_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimitInterval] = std::make_pair("", "power2_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = std::make_pair("", "power2_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = std::make_pair("", "power2_crit");
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
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageBurstPowerLimit] = SysfsValueUnit::micro;
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

std::vector<zes_power_domain_t> SysmanKmdInterfaceXe::getPowerDomains() const {
    return {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_DOMAIN_GPU, ZES_POWER_DOMAIN_MEMORY};
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

ze_result_t SysmanKmdInterfaceXe::getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                                       uint32_t engineInstance,
                                                                       uint32_t gtId,
                                                                       PmuInterface *const &pPmuInterface,
                                                                       std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                                       std::pair<uint64_t, uint64_t> &configPair) {

    ze_result_t result = ZE_RESULT_SUCCESS;

    if (isGroupEngineHandle(engineGroup)) {
        return result;
    }

    auto engineClass = engineGroupToEngineClass.find(engineGroup);
    if (engineClass == engineGroupToEngineClass.end()) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Engine Group not supported and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    const std::string sysmanDeviceDir = std::string(sysDevicesDir) + getSysmanDeviceDirName();
    uint64_t activeTicksConfig = UINT64_MAX;
    uint64_t totalTicksConfig = UINT64_MAX;

    auto ret = pPmuInterface->getPmuConfigs(sysmanDeviceDir, engineClass->second, engineInstance, gtId, activeTicksConfig, totalTicksConfig);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get configs and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    configPair = std::make_pair(activeTicksConfig, totalTicksConfig);

    int64_t fd[2];
    fd[0] = pPmuInterface->pmuInterfaceOpen(configPair.first, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
    if (fd[0] < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle \n", __FUNCTION__);
        return checkErrorNumberAndReturnStatus();
    }

    fd[1] = pPmuInterface->pmuInterfaceOpen(configPair.second, static_cast<int>(fd[0]), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
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

ze_result_t SysmanKmdInterfaceXe::getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                                   uint64_t fnNumber,
                                                                   uint64_t engineInstance,
                                                                   uint64_t engineClass,
                                                                   uint64_t gtId,
                                                                   std::pair<uint64_t, uint64_t> &configPair) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    const std::string sysmanDeviceDir = std::string(sysDevicesDir) + getSysmanDeviceDirName();

    auto ret = pPmuInterface->getPmuConfigs(sysmanDeviceDir, engineClass, engineInstance, gtId, configPair.first, configPair.second);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get configs and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    ret = pPmuInterface->getPmuConfigsForVf(sysmanDeviceDir, fnNumber, configPair.first, configPair.second);
    if (ret < 0) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get configs for VF and returning error:0x%x\n", __FUNCTION__, result);
        return result;
    }

    return result;
}

std::string SysmanKmdInterfaceXe::getGpuBindEntry() const {
    return "/sys/bus/pci/drivers/xe/bind";
}

std::string SysmanKmdInterfaceXe::getGpuUnBindEntry() const {
    return "/sys/bus/pci/drivers/xe/unbind";
}

void SysmanKmdInterfaceXe::setSysmanDeviceDirName(const bool isIntegratedDevice) {
    sysmanDeviceDirName = "xe";
    updateSysmanDeviceDirName(sysmanDeviceDirName);
}

} // namespace Sysman
} // namespace L0

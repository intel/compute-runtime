/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

SysmanKmdInterfaceI915Upstream::SysmanKmdInterfaceI915Upstream(SysmanProductHelper *pSysmanProductHelper) {
    initSysfsNameToFileMap(pSysmanProductHelper);
    initSysfsNameToNativeUnitMap(pSysmanProductHelper);
}

SysmanKmdInterfaceI915Upstream::~SysmanKmdInterfaceI915Upstream() = default;

std::string SysmanKmdInterfaceI915Upstream::getBasePath(uint32_t subDeviceId) const {
    return getBasePathI915(subDeviceId);
}

void SysmanKmdInterfaceI915Upstream::initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper) {
    sysfsNameToFileMap[SysfsName::sysfsNameMinFrequency] = std::make_pair("rps_min_freq_mhz", "gt_min_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxFrequency] = std::make_pair("rps_max_freq_mhz", "gt_max_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMinDefaultFrequency] = std::make_pair(".defaults/rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxDefaultFrequency] = std::make_pair(".defaults/rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameBoostFrequency] = std::make_pair("rps_boost_freq_mhz", "gt_boost_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameCurrentFrequency] = std::make_pair("punit_req_freq_mhz", "gt_cur_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameTdpFrequency] = std::make_pair("rapl_PL1_freq_mhz", "rapl_PL1_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameActualFrequency] = std::make_pair("rps_act_freq_mhz", "gt_act_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameEfficientFrequency] = std::make_pair("rps_RP1_freq_mhz", "gt_RP1_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxValueFrequency] = std::make_pair("rps_RP0_freq_mhz", "gt_RP0_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameMinValueFrequency] = std::make_pair("rps_RPn_freq_mhz", "gt_RPn_freq_mhz");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonStatus] = std::make_pair("throttle_reason_status", "gt_throttle_reason_status");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL1] = std::make_pair("throttle_reason_pl1", "gt_throttle_reason_status_pl1");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL2] = std::make_pair("throttle_reason_pl2", "gt_throttle_reason_status_pl2");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL4] = std::make_pair("throttle_reason_pl4", "gt_throttle_reason_status_pl4");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonThermal] = std::make_pair("throttle_reason_thermal", "gt_throttle_reason_status_thermal");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = std::make_pair("", "power1_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageSustainedPowerLimitInterval] = std::make_pair("", "power1_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode] = std::make_pair("", "energy1_input");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = std::make_pair("", "power1_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = std::make_pair("", pSysmanProductHelper->getPackageCriticalPowerLimitFile());
    sysfsNameToFileMap[SysfsName::sysfsNameStandbyModeControl] = std::make_pair("rc6_enable", "power/rc6_enable");
    sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange] = std::make_pair("addr_range", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxMemoryFrequency] = std::make_pair("mem_RP0_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinMemoryFrequency] = std::make_pair("mem_RPn_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeout] = std::make_pair("", "preempt_timeout_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeslice] = std::make_pair("", "timeslice_duration_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = std::make_pair("", "heartbeat_interval_ms");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactor] = std::make_pair("base_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactorScale] = std::make_pair("base_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactor] = std::make_pair("media_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale] = std::make_pair("media_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceSystemPowerBalance] = std::make_pair("", "sys_pwr_balance");
}

void SysmanKmdInterfaceI915Upstream::initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper) {
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeout] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeslice] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = pSysmanProductHelper->getPackageCriticalPowerLimitNativeUnit();
}

std::string SysmanKmdInterfaceI915Upstream::getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool prefixBaseDirectory) {
    if (sysfsNameToFileMap.find(sysfsName) != sysfsNameToFileMap.end()) {
        std::string filePath = prefixBaseDirectory ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
        return filePath;
    }
    // All sysfs accesses are expected to be covered
    DEBUG_BREAK_IF(1);
    return {};
}

std::string SysmanKmdInterfaceI915Upstream::getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) {
    std::string filePathPhysicalMemorySize = getBasePath(subDeviceId) +
                                             sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange].first;
    return filePathPhysicalMemorySize;
}

std::string SysmanKmdInterfaceI915Upstream::getEnergyCounterNodeFile(zes_power_domain_t powerDomain) {
    std::string filePath = {};
    if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        filePath = sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode].second;
    }
    return filePath;
}

ze_result_t SysmanKmdInterfaceI915Upstream::getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                                                 uint32_t engineInstance,
                                                                                 uint32_t gtId,
                                                                                 PmuInterface *const &pPmuInterface,
                                                                                 std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                                                 std::pair<uint64_t, uint64_t> &configPair) {
    uint64_t config = UINT64_MAX;
    auto engineClass = engineGroupToEngineClass.find(engineGroup);
    config = I915_PMU_ENGINE_BUSY(engineClass->second, engineInstance);
    configPair = std::make_pair(config, UINT64_MAX);
    auto fd = pPmuInterface->pmuInterfaceOpen(configPair.first, -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
    if (fd < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle \n", __FUNCTION__);
        return checkErrorNumberAndReturnStatus();
    }
    fdList.push_back(std::make_pair(fd, -1));
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysmanKmdInterfaceI915Upstream::readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) {

    uint64_t data[2] = {};
    auto ret = pPmuInterface->pmuRead(static_cast<int>(fdPair.first), data, sizeof(data));
    if (ret < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    pStats->activeTime = data[0] / microSecondsToNanoSeconds;
    pStats->timestamp = data[1] / microSecondsToNanoSeconds;
    return ZE_RESULT_SUCCESS;
}

std::string SysmanKmdInterfaceI915Upstream::getHwmonName(uint32_t subDeviceId, bool isSubdevice) const {
    return getHwmonNameI915(subDeviceId, isSubdevice);
}

std::string SysmanKmdInterfaceI915Upstream::getEngineBasePath(uint32_t subDeviceId) const {
    return getEngineBasePathI915(subDeviceId);
}

std::optional<std::string> SysmanKmdInterfaceI915Upstream::getEngineClassString(uint16_t engineClass) {
    return getEngineClassStringI915(engineClass);
}

ze_result_t SysmanKmdInterfaceI915Upstream::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                                         LinuxSysmanImp *pLinuxSysmanImp,
                                                                         SysFsAccessInterface *pSysfsAccess,
                                                                         ze_bool_t onSubdevice,
                                                                         uint32_t subdeviceId) {
    return getNumEngineTypeAndInstancesForDevice(getEngineBasePath(subdeviceId), mapOfEngines, pSysfsAccess);
}

void SysmanKmdInterfaceI915Upstream::getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) {
    getWedgedStatusImpl(pLinuxSysmanImp, pState);
}

void SysmanKmdInterfaceI915Upstream::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {

    auto pFsAccess = getFsAccess();
    const std::string srcVersionFile("/sys/module/i915/srcversion");
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

ze_result_t SysmanKmdInterfaceI915Upstream::getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                                             uint64_t fnNumber,
                                                                             uint64_t engineInstance,
                                                                             uint64_t engineClass,
                                                                             uint64_t gtId,
                                                                             std::pair<uint64_t, uint64_t> &configPair) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::string SysmanKmdInterfaceI915Upstream::getGpuBindEntry() const {
    return getGpuBindEntryI915();
}

std::string SysmanKmdInterfaceI915Upstream::getGpuUnBindEntry() const {
    return getGpuUnBindEntryI915();
}

void SysmanKmdInterfaceI915Upstream::setSysmanDeviceDirName(const bool isIntegratedDevice) {
    sysmanDeviceDirName = "i915";
    if (!isIntegratedDevice) {
        updateSysmanDeviceDirName(sysmanDeviceDirName);
    }
}

} // namespace Sysman
} // namespace L0

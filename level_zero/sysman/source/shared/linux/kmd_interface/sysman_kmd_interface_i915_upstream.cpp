/*
 * Copyright (C) 2023-2024 Intel Corporation
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
    sysfsNameToFileMap[SysfsName::sysfsNameSustainedPowerLimit] = std::make_pair("", "power1_max");
    sysfsNameToFileMap[SysfsName::sysfsNameSustainedPowerLimitInterval] = std::make_pair("", "power1_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNameEnergyCounterNode] = std::make_pair("", "energy1_input");
    sysfsNameToFileMap[SysfsName::sysfsNameDefaultPowerLimit] = std::make_pair("", "power1_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNameCriticalPowerLimit] = std::make_pair("", pSysmanProductHelper->getCardCriticalPowerLimitFile());
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
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSustainedPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameDefaultPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameCriticalPowerLimit] = pSysmanProductHelper->getCardCriticalPowerLimitNativeUnit();
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

int64_t SysmanKmdInterfaceI915Upstream::getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface) {
    uint64_t config = UINT64_MAX;
    auto engineClass = engineGroupToEngineClass.find(engineGroup);
    config = I915_PMU_ENGINE_BUSY(engineClass->second, engineInstance);
    return pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
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

uint32_t SysmanKmdInterfaceI915Upstream::getEventType(const bool isIntegratedDevice) {
    std::string i915DirName = "i915";
    return getEventTypeImpl(i915DirName, isIntegratedDevice);
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

ze_result_t SysmanKmdInterfaceI915Upstream::getBusyAndTotalTicksConfigs(uint64_t fnNumber, uint64_t engineInstance, uint64_t engineClass, std::pair<uint64_t, uint64_t> &configPair) {
    return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
}

} // namespace Sysman
} // namespace L0

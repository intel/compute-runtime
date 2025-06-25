/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

SysmanKmdInterfaceI915Prelim::SysmanKmdInterfaceI915Prelim(SysmanProductHelper *pSysmanProductHelper) {
    initSysfsNameToFileMap(pSysmanProductHelper);
    initSysfsNameToNativeUnitMap(pSysmanProductHelper);
}

SysmanKmdInterfaceI915Prelim::~SysmanKmdInterfaceI915Prelim() = default;

static const std::map<__u16, std::string> i915EngineClassToSysfsEngineMap = {
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, "rcs"},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COMPUTE), "ccs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, "bcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, "vcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

void SysmanKmdInterfaceI915Prelim::initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper) {
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
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactor] = std::make_pair("base_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceBaseFrequencyFactorScale] = std::make_pair("base_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactor] = std::make_pair("media_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale] = std::make_pair("media_freq_factor.scale", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceSystemPowerBalance] = std::make_pair("", "sys_pwr_balance");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeout] = std::make_pair("", "preempt_timeout_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeslice] = std::make_pair("", "timeslice_duration_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = std::make_pair("", "heartbeat_interval_ms");
}

void SysmanKmdInterfaceI915Prelim::initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper) {
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeout] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerTimeslice] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = SysfsValueUnit::milli;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageSustainedPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageDefaultPowerLimit] = SysfsValueUnit::micro;
    sysfsNameToNativeUnitMap[SysfsName::sysfsNamePackageCriticalPowerLimit] = pSysmanProductHelper->getPackageCriticalPowerLimitNativeUnit();
}

std::string SysmanKmdInterfaceI915Prelim::getBasePath(uint32_t subDeviceId) const {
    return getBasePathI915(subDeviceId);
}

std::string SysmanKmdInterfaceI915Prelim::getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool prefixBaseDirectory) {
    if (sysfsNameToFileMap.find(sysfsName) != sysfsNameToFileMap.end()) {
        std::string filePath = prefixBaseDirectory ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
        return filePath;
    }
    // All sysfs accesses are expected to be covered
    DEBUG_BREAK_IF(1);
    return {};
}

std::string SysmanKmdInterfaceI915Prelim::getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) {
    std::string filePathPhysicalMemorySize = getBasePath(subDeviceId) +
                                             sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange].first;
    return filePathPhysicalMemorySize;
}

std::string SysmanKmdInterfaceI915Prelim::getEnergyCounterNodeFile(zes_power_domain_t powerDomain) {
    std::string filePath = {};
    if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        filePath = sysfsNameToFileMap[SysfsName::sysfsNamePackageEnergyCounterNode].second;
    }
    return filePath;
}

ze_result_t SysmanKmdInterfaceI915Prelim::getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                                               uint32_t engineInstance,
                                                                               uint32_t gtId,
                                                                               PmuInterface *const &pPmuInterface,
                                                                               std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                                               std::pair<uint64_t, uint64_t> &configPair) {
    uint64_t config = UINT64_MAX;
    switch (engineGroup) {
    case ZES_ENGINE_GROUP_ALL:
        config = __PRELIM_I915_PMU_ANY_ENGINE_GROUP_BUSY_TICKS(gtId);
        break;
    case ZES_ENGINE_GROUP_COMPUTE_ALL:
    case ZES_ENGINE_GROUP_RENDER_ALL:
        config = __PRELIM_I915_PMU_RENDER_GROUP_BUSY_TICKS(gtId);
        break;
    case ZES_ENGINE_GROUP_COPY_ALL:
        config = __PRELIM_I915_PMU_COPY_GROUP_BUSY_TICKS(gtId);
        break;
    case ZES_ENGINE_GROUP_MEDIA_ALL:
        config = __PRELIM_I915_PMU_MEDIA_GROUP_BUSY_TICKS(gtId);
        break;
    default:
        auto i915EngineClass = engineGroupToEngineClass.find(engineGroup);
        config = PRELIM_I915_PMU_ENGINE_BUSY_TICKS(i915EngineClass->second, engineInstance);
        break;
    }

    int64_t fd[2];

    if (isGroupEngineHandle(engineGroup)) {
        configPair = std::make_pair(config, __PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(gtId));
    } else {
        auto i915EngineClass = engineGroupToEngineClass.find(engineGroup);
        configPair = std::make_pair(config, PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(i915EngineClass->second, engineInstance));
    }

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
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysmanKmdInterfaceI915Prelim::readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) {
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

std::string SysmanKmdInterfaceI915Prelim::getHwmonName(uint32_t subDeviceId, bool isSubdevice) const {
    return getHwmonNameI915(subDeviceId, isSubdevice);
}

std::string SysmanKmdInterfaceI915Prelim::getEngineBasePath(uint32_t subDeviceId) const {
    return getEngineBasePathI915(subDeviceId);
}

ze_result_t SysmanKmdInterfaceI915Prelim::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
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

std::optional<std::string> SysmanKmdInterfaceI915Prelim::getEngineClassString(uint16_t engineClass) {
    auto sysfEngineString = i915EngineClassToSysfsEngineMap.find(engineClass);
    if (sysfEngineString == i915EngineClassToSysfsEngineMap.end()) {
        DEBUG_BREAK_IF(true);
        return {};
    }
    return sysfEngineString->second;
}

void SysmanKmdInterfaceI915Prelim::getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) {
    getWedgedStatusImpl(pLinuxSysmanImp, pState);
}

void SysmanKmdInterfaceI915Prelim::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {

    auto pFsAccess = getFsAccess();
    const std::string agamaVersionFile("/sys/module/i915/agama_version");
    std::string strVal = {};
    ze_result_t result = pFsAccess->read(agamaVersionFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read driver version from %s and returning error:0x%x \n", __FUNCTION__, agamaVersionFile.c_str(), result);
        std::strncpy(driverVersion, unknown.data(), ZES_STRING_PROPERTY_SIZE);
    } else {
        std::strncpy(driverVersion, strVal.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    return;
}

ze_result_t SysmanKmdInterfaceI915Prelim::getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                                           uint64_t fnNumber,
                                                                           uint64_t engineInstance,
                                                                           uint64_t engineClass,
                                                                           uint64_t gtId,
                                                                           std::pair<uint64_t, uint64_t> &configPair) {

    configPair.first = ___PRELIM_I915_PMU_FN_EVENT(PRELIM_I915_PMU_ENGINE_BUSY_TICKS(engineClass, engineInstance), fnNumber);
    configPair.second = ___PRELIM_I915_PMU_FN_EVENT(PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(engineClass, engineInstance), fnNumber);
    return ZE_RESULT_SUCCESS;
}

std::string SysmanKmdInterfaceI915Prelim::getGpuBindEntry() const {
    return getGpuBindEntryI915();
}

std::string SysmanKmdInterfaceI915Prelim::getGpuUnBindEntry() const {
    return getGpuUnBindEntryI915();
}

void SysmanKmdInterfaceI915Prelim::setSysmanDeviceDirName(const bool isIntegratedDevice) {
    sysmanDeviceDirName = "i915";
    if (!isIntegratedDevice) {
        updateSysmanDeviceDirName(sysmanDeviceDirName);
    }
}

} // namespace Sysman
} // namespace L0
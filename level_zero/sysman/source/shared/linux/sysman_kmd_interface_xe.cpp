/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

#include "xe_drm.h"

namespace L0 {
namespace Sysman {

static const std::map<__u16, std::string> xeEngineClassToSysfsEngineMap = {
    {DRM_XE_ENGINE_CLASS_RENDER, "rcs"},
    {DRM_XE_ENGINE_CLASS_COMPUTE, "ccs"},
    {DRM_XE_ENGINE_CLASS_COPY, "bcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_DECODE, "vcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

SysmanKmdInterfaceXe::SysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily) {
    initSysfsNameToFileMap(productFamily);
}

SysmanKmdInterfaceXe::~SysmanKmdInterfaceXe() = default;

std::string SysmanKmdInterfaceXe::getBasePath(uint32_t subDeviceId) const {
    return "device/tile" + std::to_string(subDeviceId) + "/gt" + std::to_string(subDeviceId) + "/";
}

void SysmanKmdInterfaceXe::initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily) {
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
    sysfsNameToFileMap[SysfsName::sysfsNameSustainedPowerLimit] = std::make_pair("", "power1_max");
    sysfsNameToFileMap[SysfsName::sysfsNameSustainedPowerLimitInterval] = std::make_pair("", "power1_max_interval");
    sysfsNameToFileMap[SysfsName::sysfsNameEnergyCounterNode] = std::make_pair("", "energy1_input");
    sysfsNameToFileMap[SysfsName::sysfsNameDefaultPowerLimit] = std::make_pair("", "power1_rated_max");
    sysfsNameToFileMap[SysfsName::sysfsNameCriticalPowerLimit] = std::make_pair("", (productFamily == IGFX_PVC) ? "curr1_crit" : "power1_crit");
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

int64_t SysmanKmdInterfaceXe::getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface) {
    constexpr int64_t fd = -1;
    return fd;
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

uint32_t SysmanKmdInterfaceXe::getEventType(const bool isIntegratedDevice) {
    std::string xeDirName = "xe";
    return getEventTypeImpl(xeDirName, isIntegratedDevice);
}

void SysmanKmdInterfaceXe::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {

    auto pFsAccess = getFsAccess();
    const std::string srcVersionFile("/sys/module/xe/srcversion");
    std::string strVal = {};
    ze_result_t result = pFsAccess->read(srcVersionFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read driver version from %s and returning error:0x%x\n", __FUNCTION__, srcVersionFile.c_str(), result);
        std::strncpy(driverVersion, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    } else {
        std::strncpy(driverVersion, strVal.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    return;
}

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include "drm/i915_drm.h"
#include "drm/xe_drm.h"

namespace L0 {
namespace Sysman {

const std::string deviceDir("device");
const std::string sysDevicesDir("/sys/devices/");

SysmanKmdInterface::SysmanKmdInterface() = default;

SysmanKmdInterfaceI915::SysmanKmdInterfaceI915(const PRODUCT_FAMILY productFamily) {
    initSysfsNameToFileMap(productFamily);
}

SysmanKmdInterfaceXe::SysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily) {
    initSysfsNameToFileMap(productFamily);
}

SysmanKmdInterface::~SysmanKmdInterface() = default;
SysmanKmdInterfaceI915::~SysmanKmdInterfaceI915() = default;
SysmanKmdInterfaceXe::~SysmanKmdInterfaceXe() = default;
static const std::map<__u16, std::string> i915EngineClassToSysfsEngineMap = {
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER, "rcs"},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COMPUTE), "ccs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY, "bcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO, "vcs"},
    {drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

static const std::map<__u16, std::string> xeEngineClassToSysfsEngineMap = {
    {DRM_XE_ENGINE_CLASS_RENDER, "rcs"},
    {DRM_XE_ENGINE_CLASS_COMPUTE, "ccs"},
    {DRM_XE_ENGINE_CLASS_COPY, "bcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_DECODE, "vcs"},
    {DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, "vecs"}};

static const std::multimap<zes_engine_type_flag_t, std::string> level0EngineTypeToSysfsEngineMap = {
    {ZES_ENGINE_TYPE_FLAG_RENDER, "rcs"},
    {ZES_ENGINE_TYPE_FLAG_COMPUTE, "ccs"},
    {ZES_ENGINE_TYPE_FLAG_DMA, "bcs"},
    {ZES_ENGINE_TYPE_FLAG_MEDIA, "vcs"},
    {ZES_ENGINE_TYPE_FLAG_OTHER, "vecs"}};

static const std::map<std::string, zes_engine_type_flag_t> sysfsEngineMapToLevel0EngineType = {
    {"rcs", ZES_ENGINE_TYPE_FLAG_RENDER},
    {"ccs", ZES_ENGINE_TYPE_FLAG_COMPUTE},
    {"bcs", ZES_ENGINE_TYPE_FLAG_DMA},
    {"vcs", ZES_ENGINE_TYPE_FLAG_MEDIA},
    {"vecs", ZES_ENGINE_TYPE_FLAG_OTHER}};

std::unique_ptr<SysmanKmdInterface> SysmanKmdInterface::create(const NEO::Drm &drm) {
    std::unique_ptr<SysmanKmdInterface> pSysmanKmdInterface;
    auto drmVersion = drm.getDrmVersion(drm.getFileDescriptor());
    auto pHwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();
    const auto productFamily = pHwInfo->platform.eProductFamily;
    if ("xe" == drmVersion) {
        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceXe>(productFamily);
    } else {
        pSysmanKmdInterface = std::make_unique<SysmanKmdInterfaceI915>(productFamily);
    }

    return pSysmanKmdInterface;
}

void SysmanKmdInterface::initFsAccessInterface(const NEO::Drm &drm) {
    pFsAccess = FsAccessInterface::create();
    pProcfsAccess = ProcFsAccessInterface::create();
    std::string deviceName;
    pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), drm.getFileDescriptor(), deviceName);
    pSysfsAccess = SysFsAccessInterface::create(deviceName);
}

FsAccessInterface *SysmanKmdInterface::getFsAccess() {
    UNRECOVERABLE_IF(nullptr == pFsAccess.get());
    return pFsAccess.get();
}

ProcFsAccessInterface *SysmanKmdInterface::getProcFsAccess() {
    UNRECOVERABLE_IF(nullptr == pProcfsAccess.get());
    return pProcfsAccess.get();
}

SysFsAccessInterface *SysmanKmdInterface::getSysFsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess.get());
    return pSysfsAccess.get();
}

std::string SysmanKmdInterfaceI915::getBasePath(uint32_t subDeviceId) const {
    return "gt/gt" + std::to_string(subDeviceId) + "/";
}

std::string SysmanKmdInterfaceXe::getBasePath(uint32_t subDeviceId) const {
    return "device/tile" + std::to_string(subDeviceId) + "/gt" + std::to_string(subDeviceId) + "/";
}

void SysmanKmdInterfaceI915::initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily) {
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
    sysfsNameToFileMap[SysfsName::sysfsNameCriticalPowerLimit] = std::make_pair("", (productFamily == IGFX_PVC) ? "curr1_crit" : "power1_crit");
    sysfsNameToFileMap[SysfsName::sysfsNameStandbyModeControl] = std::make_pair("rc6_enable", "power/rc6_enable");
    sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange] = std::make_pair("addr_range", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxMemoryFrequency] = std::make_pair("mem_RP0_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinMemoryFrequency] = std::make_pair("mem_RPn_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeout] = std::make_pair("", "preempt_timeout_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerTimeslice] = std::make_pair("", "timeslice_duration_ms");
    sysfsNameToFileMap[SysfsName::sysfsNameSchedulerWatchDogTimeout] = std::make_pair("", "heartbeat_interval_ms");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactor] = std::make_pair("media_freq_factor", "");
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceMediaFrequencyFactorScale] = std::make_pair("media_freq_factor.scale", "");
}

void SysmanKmdInterfaceXe::initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily) {
    sysfsNameToFileMap[SysfsName::sysfsNameMinFrequency] = std::make_pair("rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxFrequency] = std::make_pair("rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinDefaultFrequency] = std::make_pair(".defaults/rps_min_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxDefaultFrequency] = std::make_pair(".defaults/rps_max_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameBoostFrequency] = std::make_pair("rps_boost_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameCurrentFrequency] = std::make_pair("punit_req_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameTdpFrequency] = std::make_pair("rapl_PL1_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameActualFrequency] = std::make_pair("rps_act_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameEfficientFrequency] = std::make_pair("rps_RP1_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMaxValueFrequency] = std::make_pair("rps_RP0_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameMinValueFrequency] = std::make_pair("rps_RPn_freq_mhz", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonStatus] = std::make_pair("throttle_reason_status", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL1] = std::make_pair("throttle_reason_pl1", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL2] = std::make_pair("throttle_reason_pl2", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonPL4] = std::make_pair("throttle_reason_pl4", "");
    sysfsNameToFileMap[SysfsName::sysfsNameThrottleReasonThermal] = std::make_pair("throttle_reason_thermal", "");
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
    sysfsNameToFileMap[SysfsName::sysfsNamePerformanceSystemPowerBalance] = std::make_pair("sys_pwr_balance", "");
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

std::string SysmanKmdInterfaceI915::getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool prefixBaseDirectory) {
    if (sysfsNameToFileMap.find(sysfsName) != sysfsNameToFileMap.end()) {
        std::string filePath = prefixBaseDirectory ? getBasePath(subDeviceId) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
        return filePath;
    }
    // All sysfs accesses are expected to be covered
    DEBUG_BREAK_IF(1);
    return {};
}

std::string SysmanKmdInterfaceI915::getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) {
    std::string filePathPhysicalMemorySize = getBasePath(subDeviceId) +
                                             sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange].first;
    return filePathPhysicalMemorySize;
}

std::string SysmanKmdInterfaceXe::getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) {
    std::string filePathPhysicalMemorySize = "device/tile" + std::to_string(subDeviceId) + "/" +
                                             sysfsNameToFileMap[SysfsName::sysfsNameMemoryAddressRange].first;
    return filePathPhysicalMemorySize;
}

int64_t SysmanKmdInterfaceI915::getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface) {
    uint64_t config = UINT64_MAX;
    auto engineClass = engineGroupToEngineClass.find(engineGroup);
    config = I915_PMU_ENGINE_BUSY(engineClass->second, engineInstance);
    return pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
}

int64_t SysmanKmdInterfaceXe::getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pPmuInterface) {
    uint64_t config = UINT64_MAX;

    switch (engineGroup) {
    case ZES_ENGINE_GROUP_ALL:
        config = XE_PMU_ANY_ENGINE_GROUP_BUSY(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_COMPUTE_ALL:
    case ZES_ENGINE_GROUP_RENDER_ALL:
        config = XE_PMU_RENDER_GROUP_BUSY(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_COPY_ALL:
        config = XE_PMU_COPY_GROUP_BUSY(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_MEDIA_ALL:
        config = XE_PMU_MEDIA_GROUP_BUSY(subDeviceId);
        break;
    default:
        break;
    }

    return pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
}

std::string SysmanKmdInterfaceI915::getHwmonName(uint32_t subDeviceId, bool isSubdevice) const {
    std::string filePath = isSubdevice ? "i915_gt" + std::to_string(subDeviceId) : "i915";
    return filePath;
}

std::string SysmanKmdInterfaceXe::getHwmonName(uint32_t subDeviceId, bool isSubdevice) const {
    std::string filePath = isSubdevice ? "xe_tile" + std::to_string(subDeviceId) : "xe";
    return filePath;
}

std::optional<std::string> SysmanKmdInterfaceXe::getEngineClassString(uint16_t engineClass) {
    auto sysfEngineString = xeEngineClassToSysfsEngineMap.find(engineClass);
    if (sysfEngineString == xeEngineClassToSysfsEngineMap.end()) {
        DEBUG_BREAK_IF(true);
        return {};
    }
    return sysfEngineString->second;
}

std::optional<std::string> SysmanKmdInterfaceI915::getEngineClassString(uint16_t engineClass) {
    auto sysfEngineString = i915EngineClassToSysfsEngineMap.find(static_cast<drm_i915_gem_engine_class>(engineClass));
    if (sysfEngineString == i915EngineClassToSysfsEngineMap.end()) {
        DEBUG_BREAK_IF(true);
        return {};
    }
    return sysfEngineString->second;
}

static ze_result_t getNumEngineTypeAndInstancesForSubDevices(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                             NEO::Drm *pDrm,
                                                             SysmanKmdInterface *pSysmanKmdInterface,
                                                             uint32_t subdeviceId) {
    NEO::EngineInfo *engineInfo = nullptr;
    {
        auto hwDeviceId = static_cast<SysmanHwDeviceIdDrm *>(pDrm->getHwDeviceId().get())->getSingleInstance();
        engineInfo = pDrm->getEngineInfo();
    }
    if (engineInfo == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    std::vector<NEO::EngineClassInstance> listOfEngines;
    engineInfo->getListOfEnginesOnATile(subdeviceId, listOfEngines);
    for (const auto &engine : listOfEngines) {
        std::string sysfEngineString = pSysmanKmdInterface->getEngineClassString(engine.engineClass).value_or(" ");
        if (sysfEngineString == " ") {
            continue;
        }

        std::string sysfsEngineDirNode = sysfEngineString + std::to_string(engine.engineInstance);
        auto level0EngineType = sysfsEngineMapToLevel0EngineType.find(sysfEngineString);
        auto ret = mapOfEngines.find(level0EngineType->second);
        if (ret != mapOfEngines.end()) {
            ret->second.push_back(sysfsEngineDirNode);
        } else {
            std::vector<std::string> engineVec = {};
            engineVec.push_back(sysfsEngineDirNode);
            mapOfEngines.emplace(level0EngineType->second, engineVec);
        }
    }
    return ZE_RESULT_SUCCESS;
}

static ze_result_t getNumEngineTypeAndInstancesForDevice(std::string engineDir, std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                         SysfsAccess *pSysfsAccess) {
    std::vector<std::string> localListOfAllEngines = {};
    auto result = pSysfsAccess->scanDirEntries(engineDir, localListOfAllEngines);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to scan directory entries to list all engines and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    for_each(localListOfAllEngines.begin(), localListOfAllEngines.end(),
             [&](std::string &mappedEngine) {
                 for (auto itr = level0EngineTypeToSysfsEngineMap.begin(); itr != level0EngineTypeToSysfsEngineMap.end(); itr++) {
                     char digits[] = "0123456789";
                     auto mappedEngineName = mappedEngine.substr(0, mappedEngine.find_first_of(digits, 0));
                     if (0 == mappedEngineName.compare(itr->second.c_str())) {
                         auto ret = mapOfEngines.find(itr->first);
                         if (ret != mapOfEngines.end()) {
                             ret->second.push_back(mappedEngine);
                         } else {
                             std::vector<std::string> engineVec = {};
                             engineVec.push_back(mappedEngine);
                             mapOfEngines.emplace(itr->first, engineVec);
                         }
                     }
                 }
             });
    return result;
}

ze_result_t SysmanKmdInterfaceI915::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                                 LinuxSysmanImp *pLinuxSysmanImp,
                                                                 SysfsAccess *pSysfsAccess,
                                                                 ze_bool_t onSubdevice,
                                                                 uint32_t subdeviceId) {
    return getNumEngineTypeAndInstancesForDevice(getEngineBasePath(subdeviceId), mapOfEngines, pSysfsAccess);
}

ze_result_t SysmanKmdInterfaceXe::getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                               LinuxSysmanImp *pLinuxSysmanImp,
                                                               SysfsAccess *pSysfsAccess,
                                                               ze_bool_t onSubdevice,
                                                               uint32_t subdeviceId) {
    if (onSubdevice) {
        return getNumEngineTypeAndInstancesForSubDevices(mapOfEngines,
                                                         pLinuxSysmanImp->getDrm(), pLinuxSysmanImp->getSysmanKmdInterface(), subdeviceId);
    }
    return getNumEngineTypeAndInstancesForDevice(getEngineBasePath(subdeviceId), mapOfEngines, pSysfsAccess);
}

SysmanKmdInterface::SysfsValueUnit SysmanKmdInterface::getNativeUnit(const SysfsName sysfsName) {
    auto sysfsNameToNativeUnitMap = getSysfsNameToNativeUnitMap();
    if (sysfsNameToNativeUnitMap.find(sysfsName) != sysfsNameToNativeUnitMap.end()) {
        return sysfsNameToNativeUnitMap[sysfsName];
    }
    // Entries are expected to be available at sysfsNameToNativeUnitMap
    DEBUG_BREAK_IF(true);
    return unAvailable;
}

void SysmanKmdInterface::convertSysfsValueUnit(const SysfsValueUnit dstUnit, const SysfsValueUnit srcUnit, const uint64_t srcValue, uint64_t &dstValue) const {
    dstValue = srcValue;

    if (dstUnit != srcUnit) {
        if (dstUnit == SysfsValueUnit::milliSecond && srcUnit == SysfsValueUnit::microSecond) {
            dstValue = srcValue / 1000u;
        } else if (dstUnit == SysfsValueUnit::microSecond && srcUnit == SysfsValueUnit::milliSecond) {
            dstValue = srcValue * 1000u;
        }
    }
}

uint32_t SysmanKmdInterface::getEventTypeImpl(std::string &dirName, const bool isIntegratedDevice) {
    auto pSysFsAccess = getSysFsAccess();
    auto pFsAccess = getFsAccess();

    if (!isIntegratedDevice) {
        std::string bdfDir;
        ze_result_t result = pSysFsAccess->readSymLink(deviceDir, bdfDir);
        if (ZE_RESULT_SUCCESS != result) {
            return 0;
        }
        const auto loc = bdfDir.find_last_of('/');
        auto bdf = bdfDir.substr(loc + 1);
        std::replace(bdf.begin(), bdf.end(), ':', '_');
        dirName = dirName + "_" + bdf;
    }

    const std::string eventTypeSysfsNode = sysDevicesDir + dirName + "/" + "type";
    auto eventTypeVal = 0u;
    if (ZE_RESULT_SUCCESS != pFsAccess->read(eventTypeSysfsNode, eventTypeVal)) {
        return 0;
    }
    return eventTypeVal;
}

uint32_t SysmanKmdInterfaceI915::getEventType(const bool isIntegratedDevice) {
    std::string i915DirName = "i915";
    return getEventTypeImpl(i915DirName, isIntegratedDevice);
}

uint32_t SysmanKmdInterfaceXe::getEventType(const bool isIntegratedDevice) {
    std::string xeDirName = "xe";
    return getEventTypeImpl(xeDirName, isIntegratedDevice);
}

} // namespace Sysman
} // namespace L0

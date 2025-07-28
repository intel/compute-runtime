/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
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

static const std::map<std::string, std::string> lateBindingSysfsFileToNameMap = {
    {"VRConfig", "device/lb_voltage_regulator_version"},
    {"FanTable", "device/lb_fan_control_version"}};

SysmanKmdInterfaceXe::SysmanKmdInterfaceXe(SysmanProductHelper *pSysmanProductHelper) {
    initSysfsNameToFileMap(pSysmanProductHelper);
    initSysfsNameToNativeUnitMap(pSysmanProductHelper);
}

SysmanKmdInterfaceXe::~SysmanKmdInterfaceXe() = default;

std::string SysmanKmdInterfaceXe::getBasePath(uint32_t subDeviceId) const {
    return "device/tile" + std::to_string(subDeviceId) + "/gt" + std::to_string(subDeviceId) + "/";
}

std::string SysmanKmdInterfaceXe::getBasePathForFreqDomain(uint32_t subDeviceId, zes_freq_domain_t frequencyDomainNumber) const {
    std::string basePath = "";
    if (frequencyDomainNumber == ZES_FREQ_DOMAIN_MEDIA) {
        basePath = "device/tile" + std::to_string(subDeviceId) + "/gt" + std::string(mediaDirSuffix) + "/";
    } else {
        basePath = getBasePath(subDeviceId);
    }
    return basePath;
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

std::string SysmanKmdInterfaceXe::getSysfsPathForFreqDomain(SysfsName sysfsName, uint32_t subDeviceId, bool prefixBaseDirectory,
                                                            zes_freq_domain_t frequencyDomainNumber) {
    if (sysfsNameToFileMap.find(sysfsName) != sysfsNameToFileMap.end()) {
        if (frequencyDomainNumber == ZES_FREQ_DOMAIN_MEDIA) {
            std::string filePath = prefixBaseDirectory ? getBasePathForFreqDomain(subDeviceId, frequencyDomainNumber) + sysfsNameToFileMap[sysfsName].first : sysfsNameToFileMap[sysfsName].second;
            return filePath;
        } else {
            std::string filePath = getSysfsFilePath(sysfsName, subDeviceId, prefixBaseDirectory);
            return filePath;
        }
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

std::string SysmanKmdInterfaceXe::getBurstPowerLimitFile(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) {
    return getSysfsFilePath(sysfsName, subDeviceId, false);
}

static ze_result_t getConfigs(PmuInterface *const &pPmuInterface,
                              const std::string &sysmanDeviceDir,
                              const SetOfEngineInstanceAndTileId &setEngineInstanceAndTileId,
                              zes_engine_group_t engineGroup,
                              const NEO::Drm *pDrm,
                              std::vector<uint64_t> &configs) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto engineClass = engineGroupToEngineClass.find(engineGroup);

    for (auto &engineInstanceAndTileId : setEngineInstanceAndTileId) {
        auto gtId = pDrm->getIoctlHelper()->getGtIdFromTileId(engineInstanceAndTileId.second, engineClass->second);
        uint64_t activeTicksConfig = UINT64_MAX;
        uint64_t totalTicksConfig = UINT64_MAX;

        auto ret = pPmuInterface->getPmuConfigs(sysmanDeviceDir, engineClass->second, engineInstanceAndTileId.first, gtId, activeTicksConfig, totalTicksConfig);
        if (ret < 0) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get configs and returning error:0x%x\n", __FUNCTION__, result);
            return result;
        }

        configs.push_back(activeTicksConfig);
        configs.push_back(totalTicksConfig);
    }

    return result;
}

ze_result_t SysmanKmdInterfaceXe::getPmuConfigsForGroupEngines(const MapOfEngineInfo &mapEngineInfo,
                                                               const std::string &sysmanDeviceDir,
                                                               const EngineGroupInfo &engineInfo,
                                                               PmuInterface *const &pPmuInterface,
                                                               const NEO::Drm *pDrm,
                                                               std::vector<uint64_t> &pmuConfigs) {

    const std::vector<zes_engine_group_t> singleMediaEngines = {
        ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE,
        ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
        ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE};

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto getConfigForEngine{
        [&](zes_engine_group_t engineGroup) {
            auto itrEngineInfo = mapEngineInfo.find(engineGroup);
            if (itrEngineInfo != mapEngineInfo.end()) {
                result = getConfigs(pPmuInterface, sysmanDeviceDir, itrEngineInfo->second, engineGroup, pDrm, pmuConfigs);
            }
            return result;
        }};

    switch (engineInfo.engineGroup) {
    case ZES_ENGINE_GROUP_MEDIA_ALL:
        for (auto &mediaEngineGroup : singleMediaEngines) {
            result = getConfigForEngine(mediaEngineGroup);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }
        break;

    case ZES_ENGINE_GROUP_COMPUTE_ALL:
        result = getConfigForEngine(ZES_ENGINE_GROUP_COMPUTE_SINGLE);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;

    case ZES_ENGINE_GROUP_COPY_ALL:
        result = getConfigForEngine(ZES_ENGINE_GROUP_COPY_SINGLE);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;

    case ZES_ENGINE_GROUP_RENDER_ALL:
        result = getConfigForEngine(ZES_ENGINE_GROUP_RENDER_SINGLE);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        break;

    default:
        for (auto &itrMapEngineInfo : mapEngineInfo) {
            if (!isGroupEngineHandle(itrMapEngineInfo.first)) {
                result = getConfigForEngine(itrMapEngineInfo.first);
                if (result != ZE_RESULT_SUCCESS) {
                    return result;
                }
            }
        }
    }
    return result;
}

ze_result_t SysmanKmdInterfaceXe::getPmuConfigsForSingleEngines(const std::string &sysmanDeviceDir,
                                                                const EngineGroupInfo &engineInfo,
                                                                PmuInterface *const &pPmuInterface,
                                                                const NEO::Drm *pDrm,
                                                                std::vector<uint64_t> &pmuConfigs) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    SetOfEngineInstanceAndTileId setEngineInstanceAndTileId = {{engineInfo.engineInstance, engineInfo.tileId}};

    result = getConfigs(pPmuInterface, sysmanDeviceDir, setEngineInstanceAndTileId, engineInfo.engineGroup, pDrm, pmuConfigs);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return result;
}

ze_result_t SysmanKmdInterfaceXe::readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::vector<int64_t> &fdList, zes_engine_stats_t *pStats) {

    constexpr uint32_t dataOffset = 2;
    constexpr uint32_t configTypes = 2;
    uint64_t dataCount = fdList.size();
    std::vector<uint64_t> readData(dataCount + dataOffset, 0);

    auto ret = pPmuInterface->pmuRead(static_cast<int>(fdList[0]), readData.data(), sizeof(uint64_t) * (dataCount + dataOffset));
    if (ret < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint64_t activeTime = 0u;
    uint64_t timeStamp = 0u;

    for (uint32_t i = 0u; i < dataCount; i++) {
        i % configTypes ? timeStamp += (readData[dataOffset + i] ? readData[dataOffset + i] : SysmanDevice::getSysmanTimestamp()) : activeTime += readData[dataOffset + i];
    }

    uint64_t engineCount = fdList.size() / configTypes;
    pStats->activeTime = activeTime / engineCount;
    pStats->timestamp = timeStamp / engineCount;

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

std::string SysmanKmdInterfaceXe::getFreqMediaDomainBasePath() {
    return "device/tile0/gt1/";
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

ze_result_t SysmanKmdInterfaceXe::readPcieDowngradeAttribute(std::string sysfsName, uint32_t &val) {
    std::map<std::string, std::string_view> pciSysfsNameToFileMap = {{"pcieDowngradeCapable", "device/auto_link_downgrade_capable"}, {"pcieDowngradeStatus", "device/auto_link_downgrade_status"}};
    auto key = pciSysfsNameToFileMap.find(sysfsName);
    if (key == pciSysfsNameToFileMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t result = pSysfsAccess->read(key->second.data(), val);
    return result;
}

void SysmanKmdInterfaceXe::getLateBindingSupportedFwTypes(std::vector<std::string> &fwTypes) {
    for (auto it = lateBindingSysfsFileToNameMap.begin(); it != lateBindingSysfsFileToNameMap.end(); ++it) {
        if (pSysfsAccess->canRead(it->second) == ZE_RESULT_SUCCESS) {
            fwTypes.push_back(it->first);
        }
    }
}

bool SysmanKmdInterfaceXe::isLateBindingVersionAvailable(std::string fwType, std::string &fwVersion) {
    auto key = lateBindingSysfsFileToNameMap.find(fwType);
    if (key == lateBindingSysfsFileToNameMap.end()) {
        return false;
    }
    ze_result_t result = pSysfsAccess->read(key->second.data(), fwVersion);
    return result == ZE_RESULT_SUCCESS ? true : false;
}

} // namespace Sysman
} // namespace L0

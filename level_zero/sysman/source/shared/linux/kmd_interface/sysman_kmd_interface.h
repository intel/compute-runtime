/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include "neo_igfxfmid.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {

class FsAccessInterface;
class ProcFsAccessInterface;
class SysFsAccessInterface;
class PmuInterface;
class LinuxSysmanImp;
class SysmanProductHelper;

constexpr std::string_view deviceDir("device");
constexpr std::string_view sysDevicesDir("/sys/devices/");

typedef std::pair<std::string, std::string> valuePair;

enum EngineClass {
    ENGINE_CLASS_RENDER = 0,
    ENGINE_CLASS_COPY = 1,
    ENGINE_CLASS_VIDEO = 2,
    ENGINE_CLASS_VIDEO_ENHANCE = 3,
    ENGINE_CLASS_COMPUTE = 4,
    ENGINE_CLASS_INVALID = -1
};

const std::multimap<uint16_t, zes_engine_group_t> engineClassToEngineGroup = {
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_RENDER), ZES_ENGINE_GROUP_RENDER_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COPY), ZES_ENGINE_GROUP_COPY_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COMPUTE), ZES_ENGINE_GROUP_COMPUTE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE), ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE}};

const std::multimap<zes_engine_group_t, uint16_t> engineGroupToEngineClass = {
    {ZES_ENGINE_GROUP_RENDER_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_RENDER)},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_COPY_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COPY)},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COMPUTE)},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE)}};

enum class SysfsName {
    sysfsNameMinFrequency,
    sysfsNameMaxFrequency,
    sysfsNameMinDefaultFrequency,
    sysfsNameMaxDefaultFrequency,
    sysfsNameBoostFrequency,
    sysfsNameCurrentFrequency,
    sysfsNameTdpFrequency,
    sysfsNameActualFrequency,
    sysfsNameEfficientFrequency,
    sysfsNameMaxValueFrequency,
    sysfsNameMinValueFrequency,
    sysfsNameThrottleReasonStatus,
    sysfsNameThrottleReasonPL1,
    sysfsNameThrottleReasonPL2,
    sysfsNameThrottleReasonPL4,
    sysfsNameThrottleReasonThermal,
    sysfsNameCardBurstPowerLimit,
    sysfsNameCardBurstPowerLimitInterval,
    sysfsNameCardSustainedPowerLimit,
    sysfsNameCardSustainedPowerLimitInterval,
    sysfsNameCardDefaultPowerLimit,
    sysfsNameCardCriticalPowerLimit,
    sysfsNamePackageBurstPowerLimit,
    sysfsNamePackageBurstPowerLimitInterval,
    sysfsNamePackageSustainedPowerLimit,
    sysfsNamePackageSustainedPowerLimitInterval,
    sysfsNamePackageEnergyCounterNode,
    sysfsNamePackageDefaultPowerLimit,
    sysfsNamePackageCriticalPowerLimit,
    sysfsNameCardEnergyCounterNode,
    sysfsNameStandbyModeControl,
    sysfsNameMemoryAddressRange,
    sysfsNameMaxMemoryFrequency,
    sysfsNameMinMemoryFrequency,
    sysfsNameSchedulerTimeout,
    sysfsNameSchedulerTimeslice,
    sysfsNameSchedulerWatchDogTimeout,
    sysfsNameSchedulerWatchDogTimeoutMaximum,
    sysfsNamePerformanceBaseFrequencyFactor,
    sysfsNamePerformanceMediaFrequencyFactor,
    sysfsNamePerformanceBaseFrequencyFactorScale,
    sysfsNamePerformanceMediaFrequencyFactorScale,
    sysfsNamePerformanceSystemPowerBalance,
};

enum class SysfsValueUnit {
    milli,
    micro,
    unAvailable,
};

class SysmanKmdInterface {
  public:
    SysmanKmdInterface();
    virtual ~SysmanKmdInterface();

    static std::unique_ptr<SysmanKmdInterface> create(NEO::Drm &drm, SysmanProductHelper *pSysmanProductHelper);

    virtual std::string getBasePath(uint32_t subDeviceId) const = 0;
    virtual std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) = 0;
    virtual std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) = 0;
    virtual std::string getEnergyCounterNodeFile(zes_power_domain_t powerDomain) = 0;
    virtual ze_result_t getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                             uint32_t engineInstance,
                                                             uint32_t gtId,
                                                             PmuInterface *const &pPmuInterface,
                                                             std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                             std::pair<uint64_t, uint64_t> &configPair) = 0;
    virtual ze_result_t readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) = 0;
    virtual std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const = 0;
    virtual bool isStandbyModeControlAvailable() const = 0;
    virtual bool clientInfoAvailableInFdInfo() const = 0;
    virtual bool isGroupEngineInterfaceAvailable() const = 0;
    ze_result_t initFsAccessInterface(const NEO::Drm &drm);
    virtual bool isBaseFrequencyFactorAvailable() const = 0;
    virtual bool isMediaFrequencyFactorAvailable() const = 0;
    virtual bool isSystemPowerBalanceAvailable() const = 0;
    FsAccessInterface *getFsAccess();
    ProcFsAccessInterface *getProcFsAccess();
    SysFsAccessInterface *getSysFsAccess();
    virtual std::string getEngineBasePath(uint32_t subDeviceId) const = 0;
    virtual ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                     LinuxSysmanImp *pLinuxSysmanImp,
                                                     SysFsAccessInterface *pSysfsAccess,
                                                     ze_bool_t onSubdevice,
                                                     uint32_t subdeviceId) = 0;
    ze_result_t getNumEngineTypeAndInstancesForDevice(std::string engineDir, std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                      SysFsAccessInterface *pSysfsAccess);
    ze_result_t getNumEngineTypeAndInstancesForSubDevices(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                          NEO::Drm *pDrm,
                                                          uint32_t subdeviceId);
    SysfsValueUnit getNativeUnit(const SysfsName sysfsName);
    void convertSysfsValueUnit(const SysfsValueUnit dstUnit, const SysfsValueUnit srcUnit,
                               const uint64_t srcValue, uint64_t &dstValue) const;
    virtual std::optional<std::string> getEngineClassString(uint16_t engineClass) = 0;
    uint32_t getEventType();
    virtual bool isDefaultFrequencyAvailable() const = 0;
    virtual bool isBoostFrequencyAvailable() const = 0;
    virtual bool isTdpFrequencyAvailable() const = 0;
    virtual bool isPhysicalMemorySizeSupported() const = 0;
    virtual void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) = 0;
    virtual bool isSettingTimeoutModeSupported() const = 0;
    virtual bool isSettingExclusiveModeSupported() const = 0;
    virtual void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual bool isVfEngineUtilizationSupported() const = 0;
    virtual ze_result_t getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                         uint64_t fnNumber,
                                                         uint64_t engineInstance,
                                                         uint64_t engineClass,
                                                         uint64_t gtId,
                                                         std::pair<uint64_t, uint64_t> &configPair) = 0;
    virtual std::string getGpuBindEntry() const = 0;
    virtual std::string getGpuUnBindEntry() const = 0;
    virtual std::vector<zes_power_domain_t> getPowerDomains() const = 0;
    virtual void setSysmanDeviceDirName(const bool isIntegratedDevice) = 0;
    const std::string getSysmanDeviceDirName() const;
    ze_result_t checkErrorNumberAndReturnStatus();

  protected:
    std::unique_ptr<FsAccessInterface> pFsAccess;
    std::unique_ptr<ProcFsAccessInterface> pProcfsAccess;
    std::unique_ptr<SysFsAccessInterface> pSysfsAccess;
    std::string sysmanDeviceDirName = "";
    virtual const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() = 0;
    void getWedgedStatusImpl(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState);
    void updateSysmanDeviceDirName(std::string &dirName);
};

class SysmanKmdInterfaceI915 {

  protected:
    static const std::map<uint16_t, std::string> i915EngineClassToSysfsEngineMap;
    static std::string getBasePathI915(uint32_t subDeviceId);
    static std::string getHwmonNameI915(uint32_t subDeviceId, bool isSubdevice);
    static std::optional<std::string> getEngineClassStringI915(uint16_t engineClass);
    static std::string getEngineBasePathI915(uint32_t subDeviceId);
    static std::string getGpuBindEntryI915();
    static std::string getGpuUnBindEntryI915();
};

class SysmanKmdInterfaceI915Upstream : public SysmanKmdInterface, SysmanKmdInterfaceI915 {
  public:
    SysmanKmdInterfaceI915Upstream(SysmanProductHelper *pSysmanProductHelper);
    ~SysmanKmdInterfaceI915Upstream() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    std::string getEnergyCounterNodeFile(zes_power_domain_t powerDomain) override;
    ze_result_t getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                     uint32_t engineInstance,
                                                     uint32_t gtId,
                                                     PmuInterface *const &pPmuInterface,
                                                     std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                     std::pair<uint64_t, uint64_t> &configPair) override;
    ze_result_t readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }
    bool clientInfoAvailableInFdInfo() const override { return false; }
    bool isGroupEngineInterfaceAvailable() const override { return false; }
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    bool isBaseFrequencyFactorAvailable() const override { return false; }
    bool isMediaFrequencyFactorAvailable() const override { return true; }
    bool isSystemPowerBalanceAvailable() const override { return false; }
    bool isDefaultFrequencyAvailable() const override { return true; }
    bool isBoostFrequencyAvailable() const override { return true; }
    bool isTdpFrequencyAvailable() const override { return true; }
    bool isPhysicalMemorySizeSupported() const override { return false; }
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override;
    bool isSettingTimeoutModeSupported() const override { return true; }
    bool isSettingExclusiveModeSupported() const override { return true; }
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    bool isVfEngineUtilizationSupported() const override { return false; }
    ze_result_t getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                 uint64_t fnNumber,
                                                 uint64_t engineInstance,
                                                 uint64_t engineClass,
                                                 uint64_t gtId,
                                                 std::pair<uint64_t, uint64_t> &configPair) override;
    std::string getGpuBindEntry() const override;
    std::string getGpuUnBindEntry() const override;
    std::vector<zes_power_domain_t> getPowerDomains() const override { return {ZES_POWER_DOMAIN_PACKAGE}; }
    void setSysmanDeviceDirName(const bool isIntegratedDevice) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap;
    void initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper);
    void initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
};

class SysmanKmdInterfaceI915Prelim : public SysmanKmdInterface, SysmanKmdInterfaceI915 {
  public:
    SysmanKmdInterfaceI915Prelim(SysmanProductHelper *pSysmanProductHelper);
    ~SysmanKmdInterfaceI915Prelim() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    std::string getEnergyCounterNodeFile(zes_power_domain_t powerDomain) override;
    ze_result_t getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                     uint32_t engineInstance,
                                                     uint32_t gtId,
                                                     PmuInterface *const &pPmuInterface,
                                                     std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                     std::pair<uint64_t, uint64_t> &configPair) override;
    ze_result_t readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }
    bool clientInfoAvailableInFdInfo() const override { return false; }
    bool isGroupEngineInterfaceAvailable() const override { return true; }
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    bool isBaseFrequencyFactorAvailable() const override { return true; }
    bool isMediaFrequencyFactorAvailable() const override { return true; }
    bool isSystemPowerBalanceAvailable() const override { return true; }
    bool isDefaultFrequencyAvailable() const override { return true; }
    bool isBoostFrequencyAvailable() const override { return true; }
    bool isTdpFrequencyAvailable() const override { return true; }
    bool isPhysicalMemorySizeSupported() const override { return true; }
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override;
    bool isSettingTimeoutModeSupported() const override { return true; }
    bool isSettingExclusiveModeSupported() const override { return true; }
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    bool isVfEngineUtilizationSupported() const override { return true; }
    ze_result_t getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                 uint64_t fnNumber,
                                                 uint64_t engineInstance,
                                                 uint64_t engineClass,
                                                 uint64_t gtId,
                                                 std::pair<uint64_t, uint64_t> &configPair) override;
    std::string getGpuBindEntry() const override;
    std::string getGpuUnBindEntry() const override;
    std::vector<zes_power_domain_t> getPowerDomains() const override { return {ZES_POWER_DOMAIN_PACKAGE}; }
    void setSysmanDeviceDirName(const bool isIntegratedDevice) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap;
    void initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper);
    void initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
};

class SysmanKmdInterfaceXe : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceXe(SysmanProductHelper *pSysmanProductHelper);
    ~SysmanKmdInterfaceXe() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    std::string getEnergyCounterNodeFile(zes_power_domain_t powerDomain) override;
    ze_result_t getEngineActivityFdListAndConfigPair(zes_engine_group_t engineGroup,
                                                     uint32_t engineInstance,
                                                     uint32_t gtId,
                                                     PmuInterface *const &pPmuInterface,
                                                     std::vector<std::pair<int64_t, int64_t>> &fdList,
                                                     std::pair<uint64_t, uint64_t> &configPair) override;
    ze_result_t readBusynessFromGroupFd(PmuInterface *const &pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return false; }
    bool clientInfoAvailableInFdInfo() const override { return true; }
    bool isGroupEngineInterfaceAvailable() const override { return false; }
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    bool isBaseFrequencyFactorAvailable() const override { return false; }
    bool isMediaFrequencyFactorAvailable() const override { return false; }
    bool isSystemPowerBalanceAvailable() const override { return false; }
    bool isDefaultFrequencyAvailable() const override { return false; }
    bool isBoostFrequencyAvailable() const override { return false; }
    bool isTdpFrequencyAvailable() const override { return false; }
    bool isPhysicalMemorySizeSupported() const override { return true; }
    std::vector<zes_power_domain_t> getPowerDomains() const override;

    // Wedged state is not supported in XE.
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override{};
    bool isSettingTimeoutModeSupported() const override { return false; }
    bool isSettingExclusiveModeSupported() const override { return false; }
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    bool isVfEngineUtilizationSupported() const override { return true; }
    ze_result_t getBusyAndTotalTicksConfigsForVf(PmuInterface *const &pPmuInterface,
                                                 uint64_t fnNumber,
                                                 uint64_t engineInstance,
                                                 uint64_t engineClass,
                                                 uint64_t gtId,
                                                 std::pair<uint64_t, uint64_t> &configPair) override;
    std::string getGpuBindEntry() const override;
    std::string getGpuUnBindEntry() const override;
    void setSysmanDeviceDirName(const bool isIntegratedDevice) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap;
    void initSysfsNameToFileMap(SysmanProductHelper *pSysmanProductHelper);
    void initSysfsNameToNativeUnitMap(SysmanProductHelper *pSysmanProductHelper);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    static uint64_t getPmuEngineConfig(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId);
};

} // namespace Sysman
} // namespace L0

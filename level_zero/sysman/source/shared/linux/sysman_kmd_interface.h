/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include "igfxfmid.h"

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
class SysfsAccess;

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
    sysfsNameSustainedPowerLimit,
    sysfsNameSustainedPowerLimitInterval,
    sysfsNameEnergyCounterNode,
    sysfsNameDefaultPowerLimit,
    sysfsNameCriticalPowerLimit,
    sysfsNameStandbyModeControl,
    sysfsNameMemoryAddressRange,
    sysfsNameMaxMemoryFrequency,
    sysfsNameMinMemoryFrequency,
    syfsNameSchedulerTimeout,
    syfsNameSchedulerTimeslice,
    syfsNameSchedulerWatchDogTimeout,
    syfsNameSchedulerWatchDogTimeoutMaximum,
};

class SysmanKmdInterface {
  public:
    SysmanKmdInterface();
    virtual ~SysmanKmdInterface();
    enum SysfsValueUnit : uint32_t {
        milliSecond,
        microSecond,
        unAvailable,
    };
    static std::unique_ptr<SysmanKmdInterface> create(const NEO::Drm &drm);

    virtual std::string getBasePath(uint32_t subDeviceId) const = 0;
    virtual std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) = 0;
    virtual std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) = 0;
    virtual int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) = 0;
    virtual std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const = 0;
    virtual bool isStandbyModeControlAvailable() const = 0;
    virtual bool clientInfoAvailableInFdInfo() = 0;
    virtual bool isGroupEngineInterfaceAvailable() const = 0;
    void initFsAccessInterface(const NEO::Drm &drm);
    FsAccessInterface *getFsAccess();
    ProcFsAccessInterface *getProcFsAccess();
    SysFsAccessInterface *getSysFsAccess();
    virtual std::string getEngineBasePath(uint32_t subDeviceId) const = 0;
    virtual bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() = 0;
    virtual ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                     LinuxSysmanImp *pLinuxSysmanImp,
                                                     SysfsAccess *pSysfsAccess,
                                                     ze_bool_t onSubdevice,
                                                     uint32_t subdeviceId) = 0;
    SysfsValueUnit getNativeUnit(const SysfsName sysfsName);
    void convertSysfsValueUnit(const SysfsValueUnit dstUnit, const SysfsValueUnit srcUnit,
                               const uint64_t srcValue, uint64_t &dstValue) const;
    virtual std::optional<std::string> getEngineClassString(uint16_t engineClass) = 0;
    virtual uint32_t getEventType(const bool isIntegratedDevice) = 0;

  protected:
    std::unique_ptr<FsAccessInterface> pFsAccess;
    std::unique_ptr<ProcFsAccessInterface> pProcfsAccess;
    std::unique_ptr<SysFsAccessInterface> pSysfsAccess;
    virtual const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() = 0;
    uint32_t getEventTypeImpl(std::string &dirName, const bool isIntegratedDevice);
};

class SysmanKmdInterfaceI915 : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceI915(const PRODUCT_FAMILY productFamily);
    ~SysmanKmdInterfaceI915() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }
    bool clientInfoAvailableInFdInfo() override;
    bool isGroupEngineInterfaceAvailable() const override { return false; }
    std::string getEngineBasePath(uint32_t subDeviceId) const override { return "engine"; };
    bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() override { return false; };
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysfsAccess *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    uint32_t getEventType(const bool isIntegratedDevice) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    const std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap = {
        {SysfsName::syfsNameSchedulerTimeout, milliSecond},
        {SysfsName::syfsNameSchedulerTimeslice, milliSecond},
        {SysfsName::syfsNameSchedulerWatchDogTimeout, milliSecond},
    };
};

class SysmanKmdInterfaceXe : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily);
    ~SysmanKmdInterfaceXe() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    std::string getEngineBasePath(uint32_t subDeviceId) const override { return getBasePath(subDeviceId) + "engines"; };
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return false; }
    bool clientInfoAvailableInFdInfo() override;
    bool isGroupEngineInterfaceAvailable() const override { return true; }
    bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() override { return true; };
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysfsAccess *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    uint32_t getEventType(const bool isIntegratedDevice) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    const std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap = {
        {SysfsName::syfsNameSchedulerTimeout, microSecond},
        {SysfsName::syfsNameSchedulerTimeslice, microSecond},
        {SysfsName::syfsNameSchedulerWatchDogTimeout, milliSecond},
        {SysfsName::syfsNameSchedulerWatchDogTimeoutMaximum, milliSecond},
    };
};

} // namespace Sysman
} // namespace L0

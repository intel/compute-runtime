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

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {

class PmuInterface;
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
};

class SysmanKmdInterface {
  public:
    virtual ~SysmanKmdInterface() = default;
    static std::unique_ptr<SysmanKmdInterface> create(const NEO::Drm &drm);

    virtual std::string getBasePath(uint32_t subDeviceId) const = 0;
    virtual std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) = 0;
    virtual std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) = 0;
    virtual int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) = 0;
    virtual std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const = 0;
    virtual bool isStandbyModeControlAvailable() const = 0;
};

class SysmanKmdInterfaceI915 : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceI915(const PRODUCT_FAMILY productFamily) { initSysfsNameToFileMap(productFamily); }
    ~SysmanKmdInterfaceI915() override = default;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
};

class SysmanKmdInterfaceXe : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily) { initSysfsNameToFileMap(productFamily); }
    ~SysmanKmdInterfaceXe() override = default;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return false; }

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
};

} // namespace Sysman
} // namespace L0
/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <map>
#include <memory>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {

typedef std::pair<std::string, std::string> valuePair;

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
};

class SysmanKmdInterface {
  public:
    virtual ~SysmanKmdInterface() = default;
    static std::unique_ptr<SysmanKmdInterface> create(const NEO::Drm &drm);

    virtual std::string getBasePath(int subDeviceId) const = 0;
    virtual std::string getSysfsFilePath(SysfsName sysfsName, int subDeviceId, bool baseDirectoryExists) = 0;
};

class SysmanKmdInterfaceI915 : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceI915() { initSysfsNameToFileMap(); }
    ~SysmanKmdInterfaceI915() override = default;

    std::string getBasePath(int subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, int subDeviceId, bool baseDirectoryExists) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap();
};

class SysmanKmdInterfaceXe : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceXe() { initSysfsNameToFileMap(); }
    ~SysmanKmdInterfaceXe() override = default;

    std::string getBasePath(int subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, int subDeviceId, bool baseDirectoryExists) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap();
};

} // namespace Sysman
} // namespace L0
/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace NEO {

struct HardwareInfo;
class OSInterface;

struct PhysicalDevicePciBusInfo {
    PhysicalDevicePciBusInfo(uint32_t domain, uint32_t bus, uint32_t device, uint32_t function)
        : pciDomain(domain), pciBus(bus), pciDevice(device), pciFunction(function) {}

    uint32_t pciDomain;
    uint32_t pciBus;
    uint32_t pciDevice;
    uint32_t pciFunction;

    static const uint32_t InvalidValue = std::numeric_limits<uint32_t>::max();
};

class DriverInfo {
  public:
    DriverInfo()
        : pciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue) {}

    static DriverInfo *create(const HardwareInfo *hwInfo, const OSInterface *osInterface);

    virtual ~DriverInfo() = default;

    virtual std::string getDeviceName(std::string defaultName) { return defaultName; }
    virtual std::string getVersion(std::string defaultVersion) { return defaultVersion; }
    virtual bool getMediaSharingSupport() { return true; }
    virtual bool getImageSupport() { return true; }
    virtual PhysicalDevicePciBusInfo getPciBusInfo() { return pciBusInfo; }

  protected:
    PhysicalDevicePciBusInfo pciBusInfo;
};

} // namespace NEO

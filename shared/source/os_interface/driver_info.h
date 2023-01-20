/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#include <limits>
#include <string>

namespace NEO {

struct HardwareInfo;
class OSInterface;

struct PhysicalDevicePciBusInfo {
    PhysicalDevicePciBusInfo() = default;

    PhysicalDevicePciBusInfo(uint32_t domain, uint32_t bus, uint32_t device, uint32_t function)
        : pciDomain(domain), pciBus(bus), pciDevice(device), pciFunction(function) {}

    static constexpr uint32_t invalidValue = std::numeric_limits<uint32_t>::max();
    static constexpr PhysicalDevicePciBusInfo invalid() { return {}; }

    uint32_t pciDomain = invalidValue;
    uint32_t pciBus = invalidValue;
    uint32_t pciDevice = invalidValue;
    uint32_t pciFunction = invalidValue;
};

struct PhysicalDevicePciSpeedInfo {
    static constexpr int32_t unknown = -1;
    int32_t genVersion = unknown;
    int32_t width = unknown;
    int64_t maxBandwidth = unknown;
};

enum class DriverInfoType { UNKNOWN,
                            WINDOWS,
                            LINUX };
class DriverInfo {
  public:
    DriverInfo(DriverInfoType driverInfoType)
        : driverInfoType(driverInfoType), pciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue) {}

    static DriverInfo *create(const HardwareInfo *hwInfo, const OSInterface *osInterface);

    virtual ~DriverInfo() = default;

    template <typename DerivedType>
    DerivedType *as() {
        UNRECOVERABLE_IF(DerivedType::driverInfoType != this->driverInfoType);
        return static_cast<DerivedType *>(this);
    }

    template <typename DerivedType>
    DerivedType *as() const {
        UNRECOVERABLE_IF(DerivedType::driverInfoType != this->driverInfoType);
        return static_cast<const DerivedType *>(this);
    }

    virtual std::string getDeviceName(std::string defaultName) { return defaultName; }
    virtual std::string getVersion(std::string defaultVersion) { return defaultVersion; }
    virtual bool getMediaSharingSupport() { return true; }
    virtual PhysicalDevicePciBusInfo getPciBusInfo() { return pciBusInfo; }

  protected:
    DriverInfoType driverInfoType;
    PhysicalDevicePciBusInfo pciBusInfo;
};

} // namespace NEO

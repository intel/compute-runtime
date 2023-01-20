/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"

namespace NEO {

class DriverInfoMock : public DriverInfo {
  public:
    DriverInfoMock() : DriverInfo(DriverInfoType::WINDOWS){};

    std::string getDeviceName(std::string defaultName) override { return deviceName; };
    std::string getVersion(std::string defaultVersion) override { return version; };

    void setDeviceName(const std::string &name) { deviceName = name; }
    void setVersion(const std::string &versionString) { version = versionString; }

    void setPciBusInfo(const PhysicalDevicePciBusInfo &info) {
        pciBusInfo.pciDomain = info.pciDomain;
        pciBusInfo.pciBus = info.pciBus;
        pciBusInfo.pciDevice = info.pciDevice;
        pciBusInfo.pciFunction = info.pciFunction;
    }

    bool getMediaSharingSupport() override {
        return false;
    }

  private:
    std::string deviceName;
    std::string version;
};

} // namespace NEO

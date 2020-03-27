/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

struct HardwareInfo;
class OSInterface;

class DriverInfo {
  public:
    static DriverInfo *create(const HardwareInfo *hwInfo, OSInterface *osInterface);

    virtual ~DriverInfo() = default;

    virtual std::string getDeviceName(std::string defaultName) { return defaultName; }
    virtual std::string getVersion(std::string defaultVersion) { return defaultVersion; }
    virtual bool getMediaSharingSupport() { return true; }
    virtual bool getImageSupport() { return true; }
};

} // namespace NEO

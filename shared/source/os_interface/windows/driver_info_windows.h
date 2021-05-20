/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/driver_info.h"

#include <functional>
#include <memory>
#include <string>

namespace NEO {

class SettingsReader;

class DriverInfoWindows : public DriverInfo {
  public:
    DriverInfoWindows(const std::string &path, const PhysicalDevicePciBusInfo &pciBusInfo);
    ~DriverInfoWindows();
    std::string getDeviceName(std::string defaultName) override;
    std::string getVersion(std::string defaultVersion) override;
    bool isCompatibleDriverStore() const;
    bool getMediaSharingSupport() override;
    static std::function<std::unique_ptr<SettingsReader>(const std::string &registryPath)> createRegistryReaderFunc;

  protected:
    static std::string trimRegistryKey(std::string key);
    const std::string path;
    std::unique_ptr<SettingsReader> registryReader;
};

} // namespace NEO

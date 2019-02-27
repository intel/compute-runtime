/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/device/driver_info.h"

#include <memory>
#include <string>

namespace OCLRT {

class SettingsReader;

class DriverInfoWindows : public DriverInfo {
  public:
    std::string getDeviceName(std::string defaultName);
    std::string getVersion(std::string defaultVersion);

    void setRegistryReader(SettingsReader *reader);
    std::string trimRegistryKey(std::string key);

  protected:
    std::unique_ptr<SettingsReader> registryReader;
};

} // namespace OCLRT

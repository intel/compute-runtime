/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/debug_settings_reader.h"

#include <windows.h>

#include <stdint.h>
#include <string>

namespace NEO {
class RegistryReader : public SettingsReader {
  public:
    RegistryReader() = delete;
    RegistryReader(bool userScope, const std::string &regKey);
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    int64_t getSetting(const char *settingName, int64_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    const char *appSpecificLocation(const std::string &name) override;

  protected:
    HKEY hkeyType;
    std::string registryReadRootKey;
    void setUpProcessName();
    std::string processName;
};
} // namespace NEO

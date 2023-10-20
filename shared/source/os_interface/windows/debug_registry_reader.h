/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <stdint.h>
#include <string>

namespace NEO {

enum class DebugVarPrefix : uint8_t;

class RegistryReader : public SettingsReader {
  public:
    RegistryReader() = delete;
    RegistryReader(bool userScope, const std::string &regKey);
    bool getSettingIntCommon(const char *settingName, int64_t &value);
    bool getSettingStringCommon(const char *settingName, std::string &keyValue);
    int32_t getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) override;
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    int64_t getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) override;
    int64_t getSetting(const char *settingName, int64_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    const char *appSpecificLocation(const std::string &name) override;

  protected:
    void setUpProcessName();

    std::string registryReadRootKey;
    std::string processName;

    HKEY hkeyType;
};
} // namespace NEO

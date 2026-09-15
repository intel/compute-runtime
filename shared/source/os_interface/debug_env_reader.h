/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/debug_settings_reader.h"

namespace NEO {

class EnvironmentVariableReader : public SettingsReader {
  public:
    bool hasSetting(const char *settingName, DebugVarPrefix &type) override;
    int32_t getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) override;
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    int64_t getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) override;
    int64_t getSetting(const char *settingName, int64_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    const char *appSpecificLocation(const std::string &name) override;

    static char *getEnvironmentVariable(const char *name);
    static char *findEnvironmentVariable(const char *settingName, DebugVarPrefix &type);
};
} // namespace NEO

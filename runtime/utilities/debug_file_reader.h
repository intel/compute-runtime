/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/utilities/debug_settings_reader.h"

#include <map>
#include <sstream>
#include <stdint.h>
#include <string>

namespace NEO {

class SettingsFileReader : public SettingsReader {
  public:
    SettingsFileReader(const char *filePath = nullptr);
    ~SettingsFileReader() override;
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    const char *appSpecificLocation(const std::string &name) override;

  protected:
    void parseStream(std::istream &inputStream);
    std::map<std::string, int32_t> settingValueMap;
    std::map<std::string, std::string> settingStringMap;
};
}; // namespace NEO

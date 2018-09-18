/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/utilities/debug_settings_reader.h"

#include <string>
#include <stdint.h>
#include <Windows.h>

namespace OCLRT {
class RegistryReader : public SettingsReader {
  public:
    int32_t getSetting(const char *settingName, int32_t defaultValue) override;
    bool getSetting(const char *settingName, bool defaultValue) override;
    std::string getSetting(const char *settingName, const std::string &value) override;
    RegistryReader(bool userScope) {
        igdrclHkeyType = userScope ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    }
    RegistryReader(std::string regKey) {
        igdrclRegKey = regKey;
    }

  protected:
    HKEY igdrclHkeyType = HKEY_LOCAL_MACHINE;
    std::string igdrclRegKey = "Software\\Intel\\IGFX\\OCL";
};
} // namespace OCLRT

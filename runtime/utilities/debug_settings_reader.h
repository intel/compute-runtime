/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <fstream>
#include <stdint.h>
#include <string>

namespace OCLRT {

class SettingsReader {
  public:
    virtual ~SettingsReader() {}
    static SettingsReader *create() {
        SettingsReader *readerImpl = createFileReader();
        if (readerImpl != nullptr)
            return readerImpl;

        return createOsReader(false);
    }
    static SettingsReader *createOsReader(bool userScope);
    static SettingsReader *createOsReader(const std::string &regKey);
    static SettingsReader *createFileReader();
    virtual int32_t getSetting(const char *settingName, int32_t defaultValue) = 0;
    virtual bool getSetting(const char *settingName, bool defaultValue) = 0;
    virtual std::string getSetting(const char *settingName, const std::string &value) = 0;
    virtual const char *appSpecificLocation(const std::string &name) = 0;
    static const char *settingsFileName;
};
}; // namespace OCLRT

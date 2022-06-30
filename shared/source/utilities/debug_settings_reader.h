/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>
#include <string>

namespace NEO {

class SettingsReader {
  public:
    virtual ~SettingsReader() = default;
    static SettingsReader *create(const std::string &regKey) {
        SettingsReader *readerImpl = createFileReader();
        if (readerImpl != nullptr)
            return readerImpl;

        return createOsReader(false, regKey);
    }
    static SettingsReader *createOsReader(bool userScope, const std::string &regKey);
    static SettingsReader *createFileReader();
    virtual int32_t getSetting(const char *settingName, int32_t defaultValue) = 0;
    virtual int64_t getSetting(const char *settingName, int64_t defaultValue) = 0;
    virtual bool getSetting(const char *settingName, bool defaultValue) = 0;
    virtual std::string getSetting(const char *settingName, const std::string &value) = 0;
    virtual const char *appSpecificLocation(const std::string &name) = 0;
    static const char *settingsFileName;
    MOCKABLE_VIRTUAL char *getenv(const char *settingName);
};
}; // namespace NEO

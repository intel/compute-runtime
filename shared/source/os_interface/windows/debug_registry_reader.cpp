/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/debug_registry_reader.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <stdint.h>

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope, const std::string &regKey) {
    return new RegistryReader(userScope, regKey);
}

RegistryReader::RegistryReader(bool userScope, const std::string &regKey) : registryReadRootKey(regKey) {
    hkeyType = userScope ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    setUpProcessName();
}

void RegistryReader::setUpProcessName() {
    char buff[MAX_PATH];
    GetModuleFileNameA(nullptr, buff, MAX_PATH);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        processName = "";
    }
    processName.assign(buff);
}
const char *RegistryReader::appSpecificLocation(const std::string &name) {
    if (processName.length() > 0)
        return processName.c_str();
    return name.c_str();
}

bool RegistryReader::getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue), type) ? true : false;
}

bool RegistryReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue)) ? true : false;
}

int32_t RegistryReader::getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue), type));
}

int32_t RegistryReader::getSetting(const char *settingName, int32_t defaultValue) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue)));
}

bool RegistryReader::getSettingIntCommon(const char *settingName, int64_t &value) {
    HKEY key{};
    DWORD success = ERROR_SUCCESS;
    bool retVal = false;

    success = SysCalls::regOpenKeyExA(hkeyType,
                                      registryReadRootKey.c_str(),
                                      0,
                                      KEY_READ,
                                      &key);

    if (ERROR_SUCCESS == success) {
        DWORD size = sizeof(int64_t);
        int64_t regData;

        success = SysCalls::regQueryValueExA(key,
                                             settingName,
                                             NULL,
                                             NULL,
                                             reinterpret_cast<LPBYTE>(&regData),
                                             &size);
        if (ERROR_SUCCESS == success) {
            value = regData;
            retVal = true;
        }
        RegCloseKey(key);
    }
    return retVal;
}

int64_t RegistryReader::getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) {
    int64_t value = defaultValue;

    if (!(getSettingIntCommon(settingName, value))) {
        char *envValue;

        auto prefixString = ApiSpecificConfig::getPrefixStrings();
        auto prefixType = ApiSpecificConfig::getPrefixTypes();

        uint32_t i = 0;
        for (const auto &prefix : prefixString) {
            std::string neoKey = prefix;
            neoKey += settingName;
            envValue = IoFunctions::getenvPtr(neoKey.c_str());
            if (envValue) {
                value = atoll(envValue);
                type = prefixType[i];
                return value;
            }
            i++;
        }
    }
    type = DebugVarPrefix::none;
    return value;
}

int64_t RegistryReader::getSetting(const char *settingName, int64_t defaultValue) {
    int64_t value = defaultValue;

    if (!(getSettingIntCommon(settingName, value))) {
        const char *envValue = IoFunctions::getenvPtr(settingName);
        if (envValue) {
            value = atoll(envValue);
        }
    }

    return value;
}

bool RegistryReader::getSettingStringCommon(const char *settingName, std::string &keyValue) {
    HKEY key{};
    DWORD success = ERROR_SUCCESS;
    bool retVal = false;

    success = SysCalls::regOpenKeyExA(hkeyType,
                                      registryReadRootKey.c_str(),
                                      0,
                                      KEY_READ,
                                      &key);
    if (ERROR_SUCCESS == success) {
        DWORD regType = REG_NONE;
        DWORD regSize = 0;

        success = SysCalls::regQueryValueExA(key,
                                             settingName,
                                             NULL,
                                             &regType,
                                             NULL,
                                             &regSize);
        if (ERROR_SUCCESS == success) {
            if (regType == REG_SZ || regType == REG_MULTI_SZ) {
                auto regData = std::make_unique_for_overwrite<char[]>(regSize);
                success = SysCalls::regQueryValueExA(key,
                                                     settingName,
                                                     NULL,
                                                     &regType,
                                                     reinterpret_cast<LPBYTE>(regData.get()),
                                                     &regSize);
                if (success == ERROR_SUCCESS) {
                    keyValue.assign(regData.get());
                    retVal = true;
                }
            } else if (regType == REG_BINARY) {
                size_t charCount = regSize / sizeof(wchar_t);
                auto regData = std::make_unique_for_overwrite<wchar_t[]>(charCount);
                success = SysCalls::regQueryValueExA(key,
                                                     settingName,
                                                     NULL,
                                                     &regType,
                                                     reinterpret_cast<LPBYTE>(regData.get()),
                                                     &regSize);

                if (ERROR_SUCCESS == success) {

                    std::unique_ptr<char[]> convertedData(new char[charCount + 1]);
                    std::wcstombs(convertedData.get(), regData.get(), charCount);
                    convertedData.get()[charCount] = 0;

                    keyValue.assign(convertedData.get());
                    retVal = true;
                }
            }
        }
        RegCloseKey(key);
    }
    return retVal;
}

std::string RegistryReader::getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) {
    std::string keyValue = value;

    if (!(getSettingStringCommon(settingName, keyValue))) {

        auto prefixString = ApiSpecificConfig::getPrefixStrings();
        auto prefixType = ApiSpecificConfig::getPrefixTypes();

        uint32_t i = 0;
        for (const auto &prefix : prefixString) {
            std::string neoKey = prefix;
            neoKey += settingName;
            auto envValue = IoFunctions::getEnvironmentVariable(neoKey.c_str());

            if (envValue) {
                keyValue.assign(envValue);
                type = prefixType[i];
                return keyValue;
            }
            i++;
        }
    }
    type = DebugVarPrefix::none;
    return keyValue;
}

std::string RegistryReader::getSetting(const char *settingName, const std::string &value) {
    std::string keyValue = value;

    if (!(getSettingStringCommon(settingName, keyValue))) {
        const char *envValue = IoFunctions::getEnvironmentVariable(settingName);

        if (envValue) {
            keyValue.assign(envValue);
        }
    }
    return keyValue;
}

}; // namespace NEO

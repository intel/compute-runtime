/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/debug_registry_reader.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/source/utilities/debug_settings_reader.h"

#include <stdint.h>

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope, const std::string &regKey) {
    return new RegistryReader(userScope, regKey);
}

char *SettingsReader::getenv(const char *settingName) {
    return SysCalls::getenv(settingName);
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

bool RegistryReader::getSetting(const char *settingName, bool defaultValue) {
    return getSetting(settingName, static_cast<int32_t>(defaultValue)) ? true : false;
}

int32_t RegistryReader::getSetting(const char *settingName, int32_t defaultValue) {
    return static_cast<int32_t>(getSetting(settingName, static_cast<int64_t>(defaultValue)));
}

int64_t RegistryReader::getSetting(const char *settingName, int64_t defaultValue) {
    HKEY Key{};
    int64_t value = defaultValue;
    DWORD success = ERROR_SUCCESS;
    bool readSettingFromEnv = true;

    success = SysCalls::regOpenKeyExA(hkeyType,
                                      registryReadRootKey.c_str(),
                                      0,
                                      KEY_READ,
                                      &Key);

    if (ERROR_SUCCESS == success) {
        DWORD size = sizeof(int64_t);
        int64_t regData;

        success = SysCalls::regQueryValueExA(Key,
                                             settingName,
                                             NULL,
                                             NULL,
                                             reinterpret_cast<LPBYTE>(&regData),
                                             &size);
        if (ERROR_SUCCESS == success) {
            value = regData;
            readSettingFromEnv = false;
        }
        RegCloseKey(Key);
    }
    if (readSettingFromEnv) {
        const char *envValue = getenv(settingName);
        if (envValue) {
            value = atoi(envValue);
        }
    }

    return value;
}

std::string RegistryReader::getSetting(const char *settingName, const std::string &value) {
    HKEY Key{};
    DWORD success = ERROR_SUCCESS;
    std::string keyValue = value;
    bool readSettingFromEnv = true;

    success = SysCalls::regOpenKeyExA(hkeyType,
                                      registryReadRootKey.c_str(),
                                      0,
                                      KEY_READ,
                                      &Key);
    if (ERROR_SUCCESS == success) {
        DWORD regType = REG_NONE;
        DWORD regSize = 0;

        success = SysCalls::regQueryValueExA(Key,
                                             settingName,
                                             NULL,
                                             &regType,
                                             NULL,
                                             &regSize);
        if (ERROR_SUCCESS == success) {
            if (regType == REG_SZ || regType == REG_MULTI_SZ) {
                auto regData = std::make_unique<char[]>(regSize);
                success = SysCalls::regQueryValueExA(Key,
                                                     settingName,
                                                     NULL,
                                                     &regType,
                                                     reinterpret_cast<LPBYTE>(regData.get()),
                                                     &regSize);
                if (success == ERROR_SUCCESS) {
                    keyValue.assign(regData.get());
                    readSettingFromEnv = false;
                }
            } else if (regType == REG_BINARY) {
                size_t charCount = regSize / sizeof(wchar_t);
                auto regData = std::make_unique<wchar_t[]>(charCount);
                success = SysCalls::regQueryValueExA(Key,
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
                    readSettingFromEnv = false;
                }
            }
        }
        RegCloseKey(Key);
    }

    if (readSettingFromEnv) {
        const char *envValue = strcmp(processName.c_str(), settingName) ? getenv(settingName) : getenv("cl_cache_dir");
        if (envValue) {
            keyValue.assign(envValue);
        }
    }
    return keyValue;
}

}; // namespace NEO

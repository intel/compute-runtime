/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/debug_registry_reader.h"

#include "core/os_interface/windows/windows_wrapper.h"
#include "core/utilities/debug_settings_reader.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <stdint.h>

namespace NEO {

SettingsReader *SettingsReader::createOsReader(bool userScope, const std::string &regKey) {
    return new RegistryReader(userScope, regKey);
}

RegistryReader::RegistryReader(bool userScope, const std::string &regKey) : registryReadRootKey(regKey) {
    igdrclHkeyType = userScope ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
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
    HKEY Key;
    DWORD value = defaultValue;
    DWORD success = ERROR_SUCCESS;

    success = RegOpenKeyExA(igdrclHkeyType,
                            registryReadRootKey.c_str(),
                            0,
                            KEY_READ,
                            &Key);

    if (ERROR_SUCCESS == success) {
        DWORD regType;
        DWORD size = sizeof(ULONG);

        success = RegQueryValueExA(Key,
                                   settingName,
                                   NULL,
                                   &regType,
                                   (LPBYTE)&value,
                                   &size);
        RegCloseKey(Key);
        value = ERROR_SUCCESS == success ? value : defaultValue;
    }

    return value;
}

std::string RegistryReader::getSetting(const char *settingName, const std::string &value) {
    HKEY Key;
    DWORD success = ERROR_SUCCESS;
    std::string keyValue = value;

    success = RegOpenKeyExA(igdrclHkeyType,
                            registryReadRootKey.c_str(),
                            0,
                            KEY_READ,
                            &Key);
    if (ERROR_SUCCESS == success) {
        DWORD regType = REG_NONE;
        DWORD regSize = 0;

        success = RegQueryValueExA(Key,
                                   settingName,
                                   NULL,
                                   &regType,
                                   NULL,
                                   &regSize);
        if (success == ERROR_SUCCESS && regType == REG_SZ) {
            char *regData = new char[regSize];
            success = RegQueryValueExA(Key,
                                       settingName,
                                       NULL,
                                       &regType,
                                       (LPBYTE)regData,
                                       &regSize);
            keyValue.assign(regData);
            delete[] regData;
        } else if (success == ERROR_SUCCESS && regType == REG_BINARY) {
            std::unique_ptr<wchar_t[]> regData(new wchar_t[regSize]);
            success = RegQueryValueExA(Key,
                                       settingName,
                                       NULL,
                                       &regType,
                                       (LPBYTE)regData.get(),
                                       &regSize);

            size_t charsConverted = 0;
            std::unique_ptr<char[]> convertedData(new char[regSize]);

            wcstombs_s(&charsConverted, convertedData.get(), regSize, regData.get(), regSize);

            keyValue.assign(convertedData.get());
        }

        RegCloseKey(Key);
    }
    return keyValue;
}

}; // namespace NEO

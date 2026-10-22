/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/driver_info_windows.h"

#include "shared/source/os_interface/windows/debug_registry_reader.h"

namespace NEO {

DriverInfoWindows::DriverInfoWindows(const std::string &fullPath, const PhysicalDevicePciBusInfo &pciBusInfo)
    : DriverInfo(DriverInfoType::windowsType), path(DriverInfoWindows::trimRegistryKey(fullPath)), registryReader(createRegistryReaderFunc(path)) {
    this->pciBusInfo = pciBusInfo;
}

DriverInfoWindows::~DriverInfoWindows() = default;

std::string DriverInfoWindows::trimRegistryKey(std::string path) {
    std::string prefix("\\REGISTRY\\MACHINE\\");
    auto pos = prefix.find(prefix);
    if (pos != std::string::npos) {
        path.erase(pos, prefix.length());
    }

    return path;
}

std::string DriverInfoWindows::getDeviceName(std::string defaultName) {
    return registryReader->getSetting("HardwareInformation.AdapterString", defaultName);
}

std::string DriverInfoWindows::getVersion(std::string defaultVersion) {
    return registryReader->getSetting("DriverVersion", defaultVersion);
};

bool DriverInfoWindows::containsSetting(const char *setting) {
    return registryReader->getSetting(setting, std::string("")) != "<>";
}

decltype(DriverInfoWindows::createRegistryReaderFunc) DriverInfoWindows::createRegistryReaderFunc = [](const std::string &registryPath) -> std::unique_ptr<SettingsReader> {
    return std::make_unique<RegistryReader>(false, registryPath);
};
} // namespace NEO

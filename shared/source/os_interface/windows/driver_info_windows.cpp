/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/driver_info_windows.h"

#include "shared/source/os_interface/windows/debug_registry_reader.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

std::string getCurrentLibraryPath() {
    std::string returnValue;
    WCHAR pathW[MAX_PATH];
    char path[MAX_PATH];
    HMODULE handle = NULL;

    auto status = NEO::SysCalls::getModuleHandle(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                 reinterpret_cast<LPCWSTR>(&getCurrentLibraryPath), &handle);
    if (status != 0) {

        status = NEO::SysCalls::getModuleFileName(handle, pathW, sizeof(pathW));
        if (status != 0) {
            std::wcstombs(path, pathW, MAX_PATH);
            returnValue.append(path);
        }
    }
    return returnValue;
}

namespace NEO {

DriverInfo *DriverInfo::create(const HardwareInfo *hwInfo, const OSInterface *osInterface) {
    if (osInterface == nullptr) {
        return nullptr;
    }

    auto osInterfaceImpl = osInterface->get();
    UNRECOVERABLE_IF(osInterfaceImpl == nullptr);

    auto wddm = osInterfaceImpl->getWddm();
    UNRECOVERABLE_IF(wddm == nullptr);

    return new DriverInfoWindows(wddm->getDeviceRegistryPath(), wddm->getPciBusInfo());
};

DriverInfoWindows::DriverInfoWindows(const std::string &fullPath, const PhysicalDevicePciBusInfo &pciBusInfo)
    : path(DriverInfoWindows::trimRegistryKey(fullPath)), registryReader(createRegistryReaderFunc(path)) {
    this->pciBusInfo = pciBusInfo;
}

DriverInfoWindows::~DriverInfoWindows() = default;

std::string DriverInfoWindows::trimRegistryKey(std::string path) {
    std::string prefix("\\REGISTRY\\MACHINE\\");
    auto pos = prefix.find(prefix);
    if (pos != std::string::npos)
        path.erase(pos, prefix.length());

    return path;
}

std::string DriverInfoWindows::getDeviceName(std::string defaultName) {
    return registryReader.get()->getSetting("HardwareInformation.AdapterString", defaultName);
}

std::string DriverInfoWindows::getVersion(std::string defaultVersion) {
    return registryReader.get()->getSetting("DriverVersion", defaultVersion);
};

bool DriverInfoWindows::isCompatibleDriverStore() const {
    auto currentLibraryPath = getCurrentLibraryPath();
    auto openclDriverName = registryReader.get()->getSetting("OpenCLDriverName", std::string{});
    if (openclDriverName.empty()) {
        return false;
    }

    auto driverStorePath = registryReader.get()->getSetting("DriverStorePathForComputeRuntime", currentLibraryPath);
    return currentLibraryPath.find(driverStorePath.c_str()) == 0u;
}

bool isCompatibleDriverStore(std::string &&deviceRegistryPath) {
    DriverInfoWindows driverInfo(deviceRegistryPath, PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue, PhysicalDevicePciBusInfo::InvalidValue));
    return driverInfo.isCompatibleDriverStore();
}

decltype(DriverInfoWindows::createRegistryReaderFunc) DriverInfoWindows::createRegistryReaderFunc = [](const std::string &registryPath) -> std::unique_ptr<SettingsReader> {
    return std::make_unique<RegistryReader>(false, registryPath);
};

bool DriverInfoWindows::getMediaSharingSupport() {
    return registryReader.get()->getSetting(is64bit ? "UserModeDriverName" : "UserModeDriverNameWOW", std::string("")) != "<>";
}
} // namespace NEO

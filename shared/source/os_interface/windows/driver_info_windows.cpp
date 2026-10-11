/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/driver_info_windows.h"

#include "shared/source/os_interface/windows/debug_registry_reader.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"

#include <algorithm>
#include <cctype>
#include <string_view>

std::string getCurrentLibraryPath() {
    std::string returnValue;
    WCHAR pathW[MAX_PATH];
    char path[MAX_PATH];
    HMODULE handle = NULL;

    auto status = NEO::SysCalls::getModuleHandle(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                 reinterpret_cast<LPCWSTR>(&getCurrentLibraryPath), &handle);
    if (status != 0) {

        status = NEO::SysCalls::getModuleFileName(handle, pathW, MAX_PATH);
        if (status != 0) {
            std::wcstombs(path, pathW, MAX_PATH);
            returnValue.append(path);
        }
    }
    return returnValue;
}

namespace NEO {
namespace {

std::string_view trimToDriverStore(std::string_view path) {
    constexpr std::string_view driverStore = "\\driverstore\\";

    const auto pos = path.find(driverStore);
    if (pos == std::string_view::npos) {
        return {};
    }
    return path.substr(pos);
}

} // namespace

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

bool isCompatibleDriverStore(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    if (queryAdapterRegistry(gdi, adapter, L"OpenCLDriverName").empty()) {
        return false;
    }

    auto toLowerAndUnifyDriverStore = [](std::string input) -> std::string {
        std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return std::tolower(c); });
        auto hostDriverStorePos = input.find("\\hostdriverstore\\");
        if (hostDriverStorePos != std::string::npos) {
            input.erase(hostDriverStorePos + 1, 4);
        }
        return input;
    };
    const auto currentLibraryPath = toLowerAndUnifyDriverStore(getCurrentLibraryPath());

    const auto adapterPath = toLowerAndUnifyDriverStore(queryAdapterDriverStorePath(gdi, adapter));
    const auto adapterPackage = trimToDriverStore(adapterPath);
    if (adapterPackage.empty()) {
        return false;
    }
    if (trimToDriverStore(currentLibraryPath).starts_with(adapterPackage)) {
        return true;
    }

    const auto computeRuntimePath = queryAdapterRegistry(gdi, adapter, L"DriverStorePathForComputeRuntime");
    return currentLibraryPath.starts_with(toLowerAndUnifyDriverStore(computeRuntimePath));
}

bool DriverInfoWindows::containsSetting(const char *setting) {
    return registryReader->getSetting(setting, std::string("")) != "<>";
}

decltype(DriverInfoWindows::createRegistryReaderFunc) DriverInfoWindows::createRegistryReaderFunc = [](const std::string &registryPath) -> std::unique_ptr<SettingsReader> {
    return std::make_unique<RegistryReader>(false, registryPath);
};
} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"

#include <algorithm>
#include <cctype>
#include <string>
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

bool HwDeviceIdWddm::isDriverStoreValid() const {
    const bool isGdiOverridden = debugManager.flags.OverrideGdiPath.get() != "unk";
    if (this->umKmDataTranslator->enabled() || debugManager.flags.DoNotValidateDriverPath.get() || isGdiOverridden) {
        return true;
    }
    if (false == queryAdapterRegistry(*this->getGdi(), this->adapter, L"OpenCLDriverName").empty()) {
        auto toLowerAndUnifyDriverStore = [](std::string input) -> std::string {
            std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return std::tolower(c); });
            auto hostDriverStorePos = input.find("\\hostdriverstore\\");
            if (hostDriverStorePos != std::string::npos) {
                input.erase(hostDriverStorePos + 1, 4);
            }
            return input;
        };
        const auto currentLibraryPath = toLowerAndUnifyDriverStore(getCurrentLibraryPath());
        const auto adapterPath = toLowerAndUnifyDriverStore(queryAdapterDriverStorePath(*this->getGdi(), this->adapter));
        const auto adapterPackage = trimToDriverStore(adapterPath);
        if (false == adapterPackage.empty()) {
            if (trimToDriverStore(currentLibraryPath).starts_with(adapterPackage) ||
                currentLibraryPath.starts_with(toLowerAndUnifyDriverStore(queryAdapterRegistry(*this->getGdi(), this->adapter, L"DriverStorePathForComputeRuntime")))) {
                return true;
            }
        }
    }
    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Driver path is not a valid DriverStore path. Try running with debug key: DoNotValidateDriverPath=1.\n");
    return false;
}

} // namespace NEO

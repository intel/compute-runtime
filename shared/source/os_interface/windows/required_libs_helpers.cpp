/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/required_libs_helpers.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/os_interface/windows/os_library_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"

#include <span>
#include <string>
#include <string_view>

namespace NEO {

namespace {

std::string getModuleDirectory(HMODULE module) {
    char path[MAX_PATH];
    const auto length = SysCalls::getModuleFileNameA(module, path, MAX_PATH);
    if ((0 == length) || (length >= static_cast<DWORD>(MAX_PATH))) {
        return {};
    }

    const auto pathView = std::string_view(path, length);
    const auto lastSeparator = pathView.find_last_of("\\/");
    if (std::string_view::npos == lastSeparator) {
        return {};
    }
    return std::string(pathView.substr(0, lastSeparator + 1));
}

} // namespace

const std::span<const std::string_view> RequiredLibsHelpers::getDefaultBinarySearchPaths() {
    return {};
}

const std::span<const std::string> RequiredLibsHelpers::getOptionalBinarySearchPaths(LockableSearchPaths &optionalPathsCache) {
    auto lock = optionalPathsCache.lock();

    if (not optionalPathsCache->empty()) {
        return *optionalPathsCache;
    }

    const auto &customIgcName = debugManager.flags.IgcLibraryName.getRef();
    const char *igcName = (customIgcName != "unk") ? customIgcName.c_str() : Os::igcDllName;

    HMODULE igcModule = nullptr;
    std::string directory;

    if (0 != SysCalls::getModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, igcName, &igcModule)) {
        directory = getModuleDirectory(igcModule);
    } else {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Required libs: IGC module (%s) is not loaded, falling back to the driver directory\n", igcName);
        directory = getModuleDirectory(Windows::getModuleHINSTANCE());
    }

    if (directory.empty()) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Required libs: failed to resolve the search directory\n");
        return {};
    }

    optionalPathsCache->push_back(std::move(directory));
    return *optionalPathsCache;
}

} // namespace NEO

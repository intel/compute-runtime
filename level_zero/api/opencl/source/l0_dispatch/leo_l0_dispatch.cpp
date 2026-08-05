/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_library.h"

#include <memory>
#include <mutex>

namespace NEO {
namespace LEO {

// Every entry starts out holding the driver entry point, so until the dispatch
// is armed a call reaches exactly what it would have without the indirection.
// Taking the address of a function is a constant expression, so this needs no
// dynamic initialization and cannot be observed half filled.
#define LEO_L0_ENTRY(api) decltype(&::api) api = &::api;
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch_entries.inl"
#undef LEO_L0_ENTRY

decltype(&::zeInitDrivers) directInitDrivers = &::zeInitDrivers;

OsLibrary *loaderLibrary = nullptr;

namespace {

const char *const loaderLibraryName =
#if defined(_WIN64)
    "ze_loader.dll";
#else
    "libze_loader.so.1";
#endif

bool isLoaderDispatchEnabled() {
    return NEO::debugManager.flags.EnableLEOLoaderDispatch.get() == 1;
}

// Entries the loader does not export keep calling the driver.
void fillFromLoader(OsLibrary &library) {
#define LEO_L0_ENTRY(api)                                            \
    if (auto proc = library.getProcAddress(#api); proc != nullptr) { \
        LEO::api = reinterpret_cast<decltype(LEO::api)>(proc);       \
    }
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch_entries.inl"
#undef LEO_L0_ENTRY
}

// LEO stores Level Zero handles and casts them back to driver objects, so the
// handles it gets must be the driver ones.
//
// Whether they are depends on which dispatch the loader installed: its legacy
// one wraps every handle through per type factories, its driver DDI one passes
// them through natively. That choice needs loader interception to be on, from
// ZE_ENABLE_LOADER_INTERCEPT or from more than one driver being present, and the
// driver DDI handle path to be off, from ZE_ENABLE_LOADER_DRIVER_DDI_PATH or
// from a driver that does not advertise the extension. Either alone still gives
// native handles, so it is not worth predicting - ask.
//
// A driver handle is enough to ask with, because the wrapping legacy path also
// wraps the one zeInitDrivers returns, so it moves with the context and command
// list handles LEO actually casts. Compare one taken through the loader with one
// taken directly, and resolve the loader side from the library rather than from
// the dispatch: an entry the loader does not export still holds the driver, and
// comparing the driver against itself would pass without verifying anything.
bool loaderReturnsNativeHandles(OsLibrary &library, decltype(&::zeInitDrivers) directInitDriversFn) {
    auto loaderInitDrivers = reinterpret_cast<decltype(&::zeInitDrivers)>(library.getProcAddress("zeInitDrivers"));
    if (loaderInitDrivers == nullptr) {
        return false;
    }

    ze_init_driver_type_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC;
    desc.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;

    uint32_t viaLoaderCount = 1u;
    uint32_t viaDriverCount = 1u;
    ze_driver_handle_t viaLoader = nullptr;
    ze_driver_handle_t viaDriver = nullptr;

    if (loaderInitDrivers(&viaLoaderCount, &viaLoader, &desc) != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (directInitDriversFn(&viaDriverCount, &viaDriver, &desc) != ZE_RESULT_SUCCESS) {
        return false;
    }
    return (viaLoader != nullptr) && (viaLoader == viaDriver);
}

} // namespace

void disarmL0Dispatch() {
#define LEO_L0_ENTRY(api) LEO::api = &::api;
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch_entries.inl"
#undef LEO_L0_ENTRY
}

bool armL0Dispatch() {
    if (false == isLoaderDispatchEnabled()) {
        return false;
    }

    if (loaderLibrary == nullptr) {
        OsLibraryCreateProperties properties(loaderLibraryName);
        auto library = std::unique_ptr<OsLibrary>(OsLibrary::loadFunc(properties));
        if (library == nullptr || false == library->isLoaded()) {
            // Nothing got loaded, so there is nothing to keep and nothing for
            // the wrapper to close.
            return false;
        }
        // Kept for the process lifetime from here on. See loaderLibrary.
        loaderLibrary = library.release();
    }

    fillFromLoader(*loaderLibrary);
    if (false == loaderReturnsNativeHandles(*loaderLibrary, directInitDrivers)) {
        disarmL0Dispatch();
        return false;
    }

    return true;
}

void initL0Dispatch() {
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() { armL0Dispatch(); });
}

} // namespace LEO
} // namespace NEO

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pin.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "os_pin.h"

#include <memory>

namespace NEO {

PinContext::OsLibraryLoadPtr PinContext::osLibraryLoadFunction(NEO::OsLibrary::load);

bool PinContext::init(const std::string &gtPinOpenFunctionName) {
    auto hGtPinLibrary = std::unique_ptr<OsLibrary>(PinContext::osLibraryLoadFunction(PinContext::gtPinLibraryFilename.c_str()));

    if (hGtPinLibrary == nullptr) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library %s\n", PinContext::gtPinLibraryFilename.c_str());
        return false;
    }

    OpenGTPin_fn openGTPin = reinterpret_cast<OpenGTPin_fn>(hGtPinLibrary->getProcAddress(gtPinOpenFunctionName.c_str()));
    if (openGTPin == nullptr) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library open function symbol %s\n", gtPinOpenFunctionName.c_str());
        return false;
    }

    uint32_t openResult = openGTPin(nullptr);
    if (openResult != 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "gtpin library open %s failed with status %u\n", gtPinOpenFunctionName.c_str(), openResult);
        return false;
    }
    return true;
}

} // namespace NEO

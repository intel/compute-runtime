/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pin.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_library.h"

#include "os_pin.h"

#include <memory>

namespace NEO {

bool PinContext::init(const std::string &gtPinOpenFunctionName) {
    auto hGtPinLibrary = std::unique_ptr<OsLibrary>(OsLibrary::loadFunc({PinContext::gtPinLibraryFilename}));

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

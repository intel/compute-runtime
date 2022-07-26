/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pin.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/source/inc/ze_intel_gpu.h"

#include "os_pin.h"

const std::string gtPinOpenFunctionName = "OpenGTPin";

namespace L0 {

PinContext::OsLibraryLoadPtr PinContext::osLibraryLoadFunction(NEO::OsLibrary::load);

ze_result_t PinContext::init() {
    NEO::OsLibrary *hGtPinLibrary = nullptr;

    hGtPinLibrary = PinContext::osLibraryLoadFunction(gtPinLibraryFilename.c_str());

    if (hGtPinLibrary == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library %s\n", gtPinLibraryFilename.c_str());
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    OpenGTPin_fn openGTPin = reinterpret_cast<OpenGTPin_fn>(hGtPinLibrary->getProcAddress(gtPinOpenFunctionName.c_str()));
    if (openGTPin == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library open function symbol %s\n", gtPinOpenFunctionName.c_str());
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    uint32_t openResult = openGTPin(nullptr);
    if (openResult != 0) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "gtpin library open %s failed with status %u\n", gtPinOpenFunctionName.c_str(), openResult);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
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

void PinContext::init(ze_init_flag_t flag, ze_result_t &result) {
    std::unique_ptr<NEO::OsLibrary> hGtPinLibrary = nullptr;
    result = ZE_RESULT_SUCCESS;

    if (!getenv_tobool("ZE_ENABLE_PROGRAM_INSTRUMENTATION")) {
        return;
    }

    hGtPinLibrary.reset(NEO::OsLibrary::load(gtPinLibraryFilename.c_str()));
    if (hGtPinLibrary.get() == nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library %s\n", gtPinLibraryFilename.c_str());
        result = ZE_RESULT_ERROR_UNKNOWN;
        return;
    }

    OpenGTPin_fn openGTPin = reinterpret_cast<OpenGTPin_fn>(hGtPinLibrary.get()->getProcAddress(gtPinOpenFunctionName.c_str()));
    if (openGTPin == nullptr) {
        hGtPinLibrary.reset(nullptr);
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find gtpin library open function symbol %s\n", gtPinOpenFunctionName.c_str());
        result = ZE_RESULT_ERROR_UNKNOWN;
        return;
    }

    uint32_t openResult = openGTPin(nullptr);
    if (openResult != 0) {
        hGtPinLibrary.reset(nullptr);
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "gtpin library open %s failed with status %u\n", gtPinOpenFunctionName.c_str(), openResult);
        result = ZE_RESULT_ERROR_UNKNOWN;
        return;
    }
}

} // namespace L0

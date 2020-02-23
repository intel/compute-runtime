/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "offline_compiler/offline_compiler.h"

#include "compiler_options.h"

namespace NEO {
void OfflineCompiler::resolveExtraSettings() {
    if (deviceName == "tgllp") {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::forceEmuInt32DivRemSP);
    }
}
} // namespace NEO

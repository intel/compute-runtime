/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/source_level_debugger/source_level_debugger.h"

#include "libult/source_level_debugger_library.h"

namespace NEO {
OsLibrary *SourceLevelDebugger::loadDebugger() {
    return DebuggerLibrary::load(SourceLevelDebugger::dllName);
}

} // namespace NEO

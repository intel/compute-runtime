/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/libult/source_level_debugger_library.h"

namespace NEO {
OsLibrary *SourceLevelDebugger::loadDebugger() {
    return DebuggerLibrary::load(SourceLevelDebugger::dllName);
}

} // namespace NEO

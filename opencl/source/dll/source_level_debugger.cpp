/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/source_level_debugger/source_level_debugger.h"

using namespace std;

namespace NEO {

OsLibrary *SourceLevelDebugger::loadDebugger() {
    return OsLibrary::load(SourceLevelDebugger::dllName);
}
} // namespace NEO

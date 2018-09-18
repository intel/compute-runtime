/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/source_level_debugger/source_level_debugger.h"

using namespace std;

namespace OCLRT {

OsLibrary *SourceLevelDebugger::loadDebugger() {
    return OsLibrary::load(SourceLevelDebugger::dllName);
}
} // namespace OCLRT

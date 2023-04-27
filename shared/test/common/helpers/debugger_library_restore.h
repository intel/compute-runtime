/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/libult/source_level_debugger_library.h"

namespace NEO {

class DebuggerLibraryRestore {
  public:
    DebuggerLibraryRestore() {
        restoreActiveState = DebuggerLibrary::getDebuggerActive();
        restoreAvailableState = DebuggerLibrary::getLibraryAvailable();
    }
    ~DebuggerLibraryRestore() {
        DebuggerLibrary::clearDebuggerLibraryInterceptor();
        DebuggerLibrary::setDebuggerActive(restoreActiveState);
        DebuggerLibrary::setLibraryAvailable(restoreAvailableState);
    }

    bool restoreActiveState = false;
    bool restoreAvailableState = false;
};

} // namespace NEO

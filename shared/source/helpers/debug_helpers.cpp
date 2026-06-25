/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/abort.h"

#include <cassert>
#include <cstdio>

namespace NEO {
COLD_SECTION void debugBreak(int line, const char *file) {
    if (debugManager.flags.EnableDebugBreak.get()) {
        PRINT_STRING(true, stdout, "Assert was called at %d line in file:\n%s\n", line, file);
        fflush(stdout);
        assert(false);
    }
}
COLD_SECTION void abortUnrecoverable(int line, const char *file) {
    PRINT_STRING(true, stdout, "Abort was called at %d line in file:\n%s\n", line, file);
    fflush(stdout);
    abortExecution();
}
} // namespace NEO

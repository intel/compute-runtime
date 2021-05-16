/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <assert.h>
#include <cstdio>

namespace NEO {
void debugBreak(int line, const char *file) {
    if (DebugManager.flags.EnableDebugBreak.get()) {
        printf("Assert was called at %d line in file:\n%s\n", line, file);
        fflush(stdout);
        assert(false);
    }
}
void abortUnrecoverable(int line, const char *file) {
    printf("Abort was called at %d line in file:\n%s\n", line, file);
    fflush(stdout);
    abortExecution();
}
} // namespace NEO

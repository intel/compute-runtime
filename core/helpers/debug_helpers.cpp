/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/debug_helpers.h"

#include "core/debug_settings/debug_settings_manager.h"

#include <assert.h>
#include <cstdio>

namespace NEO {
void debugBreak(int line, const char *file) {
    if (DebugManager.flags.EnableDebugBreak.get()) {
        printf("Assert was called at %d line in file:\n%s\n", line, file);
        assert(false);
    }
}
void abortUnrecoverable(int line, const char *file) {
    printf("Abort was called at %d line in file:\n%s\n", line, file);
    abortExecution();
}
} // namespace NEO

/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"

#include <stdexcept>
#include <string>

namespace NEO {
COLD_SECTION void debugBreak(int line, const char *file) {
}
COLD_SECTION void abortUnrecoverable(int line, const char *file) {
    std::string message = "Abort was called at " + std::to_string(line) + " line in file " + file;
    throw std::runtime_error(message);
}
} // namespace NEO

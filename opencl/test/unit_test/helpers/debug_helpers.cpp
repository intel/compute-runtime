/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"

#include <stdexcept>
#include <string>

namespace NEO {
void debugBreak(int line, const char *file) {
}
void abortUnrecoverable(int line, const char *file) {
    std::string message = "Abort was called at " + std::to_string(line) + " line in file " + file;
    throw std::runtime_error(message);
}
} // namespace NEO

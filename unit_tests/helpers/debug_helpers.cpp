/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/debug_helpers.h"

#include <exception>

namespace NEO {
void debugBreak(int line, const char *file) {
}
void abortUnrecoverable(int line, const char *file) {
    throw std::exception();
}
} // namespace NEO
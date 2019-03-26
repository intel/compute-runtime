/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/program/program.h"

namespace NEO {

bool Program::isSafeToSkipUnhandledToken(unsigned int token) const {
    return false;
}

} // namespace NEO

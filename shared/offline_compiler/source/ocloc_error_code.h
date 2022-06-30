/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO::OclocErrorCode {

enum {
    SUCCESS = 0,
    OUT_OF_HOST_MEMORY = -6,
    BUILD_PROGRAM_FAILURE = -11,
    INVALID_DEVICE = -33,
    INVALID_PROGRAM = -44,
    INVALID_COMMAND_LINE = -5150,
    INVALID_FILE = -5151,
    COMPILATION_CRASH = -5152,
};

} // namespace NEO::OclocErrorCode
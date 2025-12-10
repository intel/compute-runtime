/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class TranslationErrorCode {
    success = 0,
    compilerNotAvailable,
    compilationFailure,
    buildFailure,
    linkFailure,
    alreadyCompiled,
    unknownError,
};

} // namespace NEO

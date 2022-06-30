/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class AuxTranslationDirection : uint32_t {
    None,
    AuxToNonAux,
    NonAuxToAux
};

enum class AuxTranslationMode : int32_t {
    None = 0,
    Builtin = 1,
    Blit = 2
};
} // namespace NEO

/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class AuxTranslationDirection : uint32_t {
    none,
    auxToNonAux,
    nonAuxToAux
};

enum class AuxTranslationMode : int32_t {
    none = 0,
    builtin = 1,
    blit = 2
};
} // namespace NEO

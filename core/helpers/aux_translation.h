/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

enum class AuxTranslationDirection {
    None,
    AuxToNonAux,
    NonAuxToAux
};

enum class AuxTranslationMode : int32_t {
    Builtin = 0,
    Blit = 1
};
} // namespace NEO

/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct TGLLPFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN12LP_CORE> {
    typedef TGLLPFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

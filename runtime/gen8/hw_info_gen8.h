/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"

namespace OCLRT {

struct BDWFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN8_CORE> {
    typedef BDWFamily GfxFamily;
    static const char *name;
};
} // namespace OCLRT

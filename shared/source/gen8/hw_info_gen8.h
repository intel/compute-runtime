/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct BDWFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN8_CORE> {
    typedef BDWFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

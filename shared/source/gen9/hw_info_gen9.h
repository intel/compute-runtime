/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct SKLFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN9_CORE> {
    typedef SKLFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

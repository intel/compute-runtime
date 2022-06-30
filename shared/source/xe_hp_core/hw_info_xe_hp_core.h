/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct XeHpFamily;

template <>
struct GfxFamilyMapper<IGFX_XE_HP_CORE> {
    typedef XeHpFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

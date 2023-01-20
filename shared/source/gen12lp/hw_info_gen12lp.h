/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct Gen12LpFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN12LP_CORE> {
    typedef Gen12LpFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

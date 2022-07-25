/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct Gen9Family;

template <>
struct GfxFamilyMapper<IGFX_GEN9_CORE> {
    typedef Gen9Family GfxFamily;
    static const char *name;
};
} // namespace NEO

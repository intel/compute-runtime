/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct Gen11Family;

template <>
struct GfxFamilyMapper<IGFX_GEN11_CORE> {
    typedef Gen11Family GfxFamily;
    static const char *name;
};
} // namespace NEO

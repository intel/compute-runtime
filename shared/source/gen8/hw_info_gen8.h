/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

struct Gen8Family;

template <>
struct GfxFamilyMapper<IGFX_GEN8_CORE> {
    typedef Gen8Family GfxFamily;
    static const char *name;
};
} // namespace NEO

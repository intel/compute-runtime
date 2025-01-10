/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

struct Xe3CoreFamily;

template <>
struct GfxFamilyMapper<IGFX_XE3_CORE> {
    using GfxFamily = Xe3CoreFamily;
    static const char *name;
};
} // namespace NEO

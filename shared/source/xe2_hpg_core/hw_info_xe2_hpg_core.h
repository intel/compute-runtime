/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

struct Xe2HpgCoreFamily;

template <>
struct GfxFamilyMapper<IGFX_XE2_HPG_CORE> {
    using GfxFamily = Xe2HpgCoreFamily;
    static const char *name;
};
} // namespace NEO

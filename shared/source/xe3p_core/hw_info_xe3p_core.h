/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

struct Xe3pCoreFamily;

template <>
struct GfxFamilyMapper<IGFX_XE3P_CORE> {
    using GfxFamily = Xe3pCoreFamily;
    static const char *name;
};
} // namespace NEO

/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct XeHpgCoreFamily;

template <>
struct GfxFamilyMapper<IGFX_XE_HPG_CORE> {
    typedef XeHpgCoreFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

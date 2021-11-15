/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct XE_HPG_COREFamily;

template <>
struct GfxFamilyMapper<IGFX_XE_HPG_CORE> {
    typedef XE_HPG_COREFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

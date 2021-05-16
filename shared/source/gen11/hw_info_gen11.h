/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct ICLFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN11_CORE> {
    typedef ICLFamily GfxFamily;
    static const char *name;
};
} // namespace NEO

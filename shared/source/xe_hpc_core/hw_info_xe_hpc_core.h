/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

namespace NEO {

struct XE_HPC_COREFamily;

template <>
struct GfxFamilyMapper<IGFX_XE_HPC_CORE> {
    using GfxFamily = XE_HPC_COREFamily;
    static const char *name;
};
} // namespace NEO

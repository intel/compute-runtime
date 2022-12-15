/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {

enum class ProductFamily : uint32_t {
    Bdw,
    Skl,
    Kbl,
    Cfl,
    Bxt,
    Glk,
    Icllp,
    Tgllp,
    Adls,
    Adlp,
    Adln,
    Dg1,
    XeHpSdv,
    Dg2,
    Pvc,
    Mtl,
    MaxProduct,
};
} // namespace aub_stream

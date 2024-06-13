/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {

enum class ProductFamily : uint32_t {
    Bdw = 0u,
    Skl = 1u,
    Kbl = 2u,
    Cfl = 3u,
    Bxt = 4u,
    Glk = 5u,
    Icllp = 6u,
    Tgllp = 7u,
    Adls = 8u,
    Adlp = 9u,
    Adln = 10u,
    Dg1 = 11u,
    Dg2 = 13u,
    Pvc = 14u,
    Mtl = 15u,
    Arl = 15u,
    Bmg = 17u,
    Lnl = 18u,
    MaxProduct,
};
} // namespace aub_stream

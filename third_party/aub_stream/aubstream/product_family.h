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
    Ptl = 21u,
    MaxProduct,
};
} // namespace aub_stream

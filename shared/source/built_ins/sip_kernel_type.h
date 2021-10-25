/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class SipKernelType : std::uint32_t {
    Csr = 0,
    DbgCsr,
    DbgCsrLocal,
    DbgBindless,
    COUNT
};

enum class SipClassType : std::uint32_t {
    Init = 0,
    Builtins,
    RawBinaryFromFile,
    HexadecimalHeaderFile
};

} // namespace NEO

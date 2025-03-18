/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class SipKernelType : std::uint32_t {
    csr = 0,
    dbgCsr,
    dbgCsrLocal,
    dbgBindless,
    dbgHeapless,
    count
};

enum class SipClassType : std::uint32_t {
    init = 0,
    builtins,
    rawBinaryFromFile,
    hexadecimalHeaderFile,
    externalLib
};

} // namespace NEO

/*
 * Copyright (C) 2020 Intel Corporation
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
    COUNT
};

} // namespace NEO

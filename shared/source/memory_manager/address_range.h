/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

struct AddressRange {
    uint64_t address;
    size_t size;
};

} // namespace NEO

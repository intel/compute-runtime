/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <type_traits>

namespace aub_stream {

struct PhysicalAllocationInfo {
    uint64_t physicalAddress;
    size_t size;
    uint32_t memoryBank;
    size_t pageSize;
};

static_assert(std::is_pod<PhysicalAllocationInfo>::value, "PhysicalAllocationInfo is not POD type");

} // namespace aub_stream
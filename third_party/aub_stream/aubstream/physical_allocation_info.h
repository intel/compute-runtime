/*
 * Copyright (C) 2022-2024 Intel Corporation
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

static_assert(std::is_standard_layout_v<PhysicalAllocationInfo> && std::is_trivial_v<PhysicalAllocationInfo> && std::is_trivially_copyable_v<PhysicalAllocationInfo>, "PhysicalAllocationInfo is not POD type");

} // namespace aub_stream
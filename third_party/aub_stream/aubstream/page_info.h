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

struct PageInfo {
    uint64_t physicalAddress;
    size_t size;
    bool isLocalMemory;
    uint32_t memoryBank;
};

static_assert(std::is_standard_layout_v<PageInfo> && std::is_trivial_v<PageInfo> && std::is_trivially_copyable_v<PageInfo>, "PageInfo is not POD type");

} // namespace aub_stream
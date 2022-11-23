/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace aub_stream {

struct AllocationParams {
    AllocationParams() = delete;
    AllocationParams(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize)
        : gfxAddress(gfxAddress), memory(memory), size(size), memoryBanks(memoryBanks), hint(hint), pageSize(pageSize) {
        additionalParams = {};
    }

    uint64_t gfxAddress = 0;
    const void *memory = nullptr;
    size_t size = 0;
    uint32_t memoryBanks = 0;
    int hint = 0;
    size_t pageSize = 0;

    struct AdditionalParams {
        bool compressionEnabled : 1;
        bool uncached : 1;
        bool padding : 6;
    };

    AdditionalParams additionalParams;
};

static_assert(std::is_standard_layout<AllocationParams>::value, "AllocationParams is not standard layout type");

} // namespace aub_stream

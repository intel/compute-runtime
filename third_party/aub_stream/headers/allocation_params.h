/*
* Copyright (C) 2021 Intel Corporation
*
* SPDX-License-Identifier: MIT
*
*/

#pragma once
#include <cstddef>
#include <cstdint>

namespace aub_stream {

struct AllocationParams {
    AllocationParams() = delete;
    AllocationParams(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize)
        : gfxAddress(gfxAddress), size(size), pageSize(pageSize), memoryBanks(memoryBanks), hint(hint), memory(memory) {
        additionalParams = {};
    }
    uint64_t gfxAddress = 0;
    size_t size = 0;
    size_t pageSize;
    uint32_t memoryBanks;
    int hint = 0;
    const void *memory = nullptr;

    struct AdditionalParams {
        bool compressionEnabled :1;
        bool uncached : 1;
        bool padding : 6;
    } additionalParams;
};

} // namespace aub_stream

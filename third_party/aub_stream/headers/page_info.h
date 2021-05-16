/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace aub_stream {

struct PageInfo {
    uint64_t physicalAddress;
    size_t size;
    bool isLocalMemory;
    uint32_t memoryBank;
};
} // namespace aub_stream

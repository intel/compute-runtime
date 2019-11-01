/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#pragma once
namespace aub_stream {

struct PageInfo {
    uint64_t physicalAddress;
    size_t size;
    bool isLocalMemory;
    uint32_t memoryBank;
};
} // namespace aub_stream
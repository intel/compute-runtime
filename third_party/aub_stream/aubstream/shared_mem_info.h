/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {

struct SharedMemoryInfo {
    uint8_t *sysMemBase{};
    uint64_t sysMemSize{};

    uint8_t *localMemBase{};
    uint64_t localMemSize{};
};

} // namespace aub_stream

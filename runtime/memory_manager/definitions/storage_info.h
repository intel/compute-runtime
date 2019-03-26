/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
namespace NEO {
struct StorageInfo {
    uint32_t getNumHandles() const;
};
} // namespace NEO

/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
namespace OCLRT {
struct StorageInfo {
    uint32_t getNumHandles() const;
};
} // namespace OCLRT

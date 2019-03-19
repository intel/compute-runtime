/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {

struct MemoryInfo {
    MemoryInfo() = default;
    virtual ~MemoryInfo() = 0;
};

inline MemoryInfo::~MemoryInfo(){};

} // namespace OCLRT

/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct MemoryInfo {
    MemoryInfo() = default;
    virtual ~MemoryInfo() = 0;
};

inline MemoryInfo::~MemoryInfo(){};

} // namespace NEO

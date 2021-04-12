/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

class MemoryInfo {
  public:
    MemoryInfo() = default;
    virtual ~MemoryInfo() = 0;
};

inline MemoryInfo::~MemoryInfo(){};

} // namespace NEO

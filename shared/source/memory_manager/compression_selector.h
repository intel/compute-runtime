/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct AllocationProperties;

class CompressionSelector {
  public:
    static bool preferCompressedAllocation(const AllocationProperties &properties);
};

} // namespace NEO

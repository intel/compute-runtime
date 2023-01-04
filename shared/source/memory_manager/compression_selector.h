/*
 * Copyright (C) 2020-2023 Intel Corporation
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
    static bool allowStatelessCompression();
};

} // namespace NEO

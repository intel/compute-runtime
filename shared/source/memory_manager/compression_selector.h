/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct AllocationProperties;
struct HardwareInfo;

class CompressionSelector {
  public:
    static bool preferCompressedAllocation(const AllocationProperties &properties, const HardwareInfo &hwInfo);
};

} // namespace NEO

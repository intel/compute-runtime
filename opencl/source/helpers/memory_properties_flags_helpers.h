/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_ext_private.h"

#include "memory_properties_flags.h"

namespace NEO {

class MemoryPropertiesParser {
  public:
    static void addExtraMemoryProperties(MemoryProperties &properties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel);

    static MemoryProperties createMemoryProperties(cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem_alloc_flags_intel allocflags);
};
} // namespace NEO

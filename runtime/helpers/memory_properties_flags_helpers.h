/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "public/cl_ext_private.h"

#include "memory_properties_flags.h"

namespace NEO {

class MemoryPropertiesFlagsParser {
  public:
    static void addExtraMemoryPropertiesFlags(MemoryPropertiesFlags &propertiesFlags, cl_mem_flags flags, cl_mem_flags_intel flagsIntel);

    static MemoryPropertiesFlags createMemoryPropertiesFlags(cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem_alloc_flags_intel allocflags);
};
} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/memory_properties_helpers.h"

#include "opencl/extensions/public/cl_ext_private.h"

namespace NEO {

class Context;

class ClMemoryPropertiesHelper {
  public:
    static MemoryProperties createMemoryProperties(cl_mem_flags flags, cl_mem_flags_intel flagsIntel,
                                                   cl_mem_alloc_flags_intel allocflags, const Device *pDevice);

    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                      cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel, cl_mem_alloc_flags_intel &allocflags,
                                      MemoryPropertiesHelper::ObjType objectType, Context &context);
};
} // namespace NEO

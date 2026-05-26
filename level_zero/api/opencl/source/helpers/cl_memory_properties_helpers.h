/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/unified_memory_properties.h"

#include "level_zero/api/opencl/source/extensions/public/cl_ext_private.h"

namespace NEO {
namespace LEO {

class Context;

using Device = NEO::Device;
using MemoryProperties = NEO::MemoryProperties;

class ClMemoryPropertiesHelper {
  public:
    enum class ObjType {
        unknown,
        buffer,
        image,
    };

    static MemoryProperties createMemoryProperties(cl_mem_flags flags, cl_mem_flags_intel flagsIntel, cl_mem_alloc_flags_intel allocflags, const Device *pDevice);

    static bool parseMemoryProperties(const cl_mem_properties_intel *properties, MemoryProperties &memoryProperties,
                                      cl_mem_flags &flags, cl_mem_flags_intel &flagsIntel, cl_mem_alloc_flags_intel &allocflags,
                                      ObjType objectType, Context &context);
};

} // namespace LEO
} // namespace NEO

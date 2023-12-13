/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_ext_private.h"

namespace NEO {

class Context;
class Device;
struct MemoryProperties;

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
} // namespace NEO

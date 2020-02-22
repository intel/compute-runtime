/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/mem_obj_helper_common.inl"

#include "memory_properties_flags.h"

namespace NEO {

bool MemObjHelper::isSuitableForRenderCompression(bool renderCompressed, const MemoryPropertiesFlags &properties, Context &context, bool preferCompression) {
    return renderCompressed && preferCompression;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel) {
    return true;
}

const uint64_t MemObjHelper::extraFlags = 0;

const uint64_t MemObjHelper::extraFlagsIntel = 0;

} // namespace NEO

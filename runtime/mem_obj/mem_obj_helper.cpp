/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper_common.inl"

#include "memory_properties_flags.h"

namespace NEO {

bool MemObjHelper::isSuitableForRenderCompression(bool renderCompressed, const MemoryPropertiesFlags &properties, Context &context, bool preferCompression) {
    return renderCompressed && preferCompression;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &properties) {
    return true;
}

const uint64_t MemObjHelper::extraFlags = 0;

const uint64_t MemObjHelper::extraFlagsIntel = 0;

} // namespace NEO

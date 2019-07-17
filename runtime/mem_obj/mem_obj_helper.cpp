/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"

#include "common/helpers/bit_helpers.h"

namespace NEO {

bool MemObjHelper::isSuitableForRenderCompression(bool renderCompressed, const MemoryProperties &properties, ContextType contextType, bool preferCompression) {
    return renderCompressed;
}

bool MemObjHelper::validateExtraMemoryProperties(const MemoryProperties &properties) {
    return true;
}

void MemObjHelper::addExtraMemoryProperties(MemoryProperties &properties) {
}

} // namespace NEO

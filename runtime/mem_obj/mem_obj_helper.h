/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/helpers/bit_helpers.h"
#include "public/cl_ext_private.h"
#include "runtime/context/context_type.h"
#include "runtime/helpers/mem_properties_parser_helper.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/unified_memory_manager.h"

#include "CL/cl.h"
#include "mem_obj_types.h"
#include "memory_properties_flags.h"

namespace NEO {

class MemObjHelper {
  public:
    static const uint64_t extraFlags;
    static const uint64_t extraFlagsIntel;
    static const uint64_t commonFlags;
    static const uint64_t commonFlagsIntel;
    static const uint64_t validFlagsForBuffer;
    static const uint64_t validFlagsForBufferIntel;
    static const uint64_t validFlagsForImage;
    static const uint64_t validFlagsForImageIntel;

    static bool validateMemoryPropertiesForBuffer(const MemoryProperties &properties);
    static bool validateMemoryPropertiesForImage(const MemoryProperties &properties, cl_mem parent);
    static AllocationProperties getAllocationPropertiesWithImageInfo(ImageInfo &imgInfo, bool allocateMemory, const MemoryPropertiesFlags &memoryProperties);
    static bool checkMemFlagsForSubBuffer(cl_mem_flags flags);
    static SVMAllocsManager::SvmAllocationProperties getSvmAllocationProperties(cl_mem_flags flags);
    static bool isSuitableForRenderCompression(bool renderCompressed, const MemoryPropertiesFlags &properties, ContextType contextType, bool preferCompression);

  protected:
    static bool validateExtraMemoryProperties(const MemoryProperties &properties);
};
} // namespace NEO

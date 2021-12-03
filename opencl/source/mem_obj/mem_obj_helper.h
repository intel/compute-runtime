/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/mem_obj/mem_obj.h"

#include "CL/cl.h"
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

    static bool validateMemoryPropertiesForBuffer(const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                                  cl_mem_flags_intel flagsIntel, const Context &context);
    static bool validateMemoryPropertiesForImage(const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                                 cl_mem_flags_intel flagsIntel, cl_mem parent, const Context &context);
    static AllocationProperties getAllocationPropertiesWithImageInfo(uint32_t rootDeviceIndex, ImageInfo &imgInfo, bool allocateMemory,
                                                                     const MemoryProperties &memoryProperties,
                                                                     const HardwareInfo &hwInfo, DeviceBitfield subDevicesBitfieldParam, bool deviceOnlyVisibilty);
    static bool checkMemFlagsForSubBuffer(cl_mem_flags flags);
    static SVMAllocsManager::SvmAllocationProperties getSvmAllocationProperties(cl_mem_flags flags);
    static bool isSuitableForCompression(bool compressionSupported, const MemoryProperties &properties, Context &context,
                                         bool preferCompression);

  protected:
    static bool validateExtraMemoryProperties(const MemoryProperties &memoryProperties, cl_mem_flags flags,
                                              cl_mem_flags_intel flagsIntel, const Context &context);
};
} // namespace NEO

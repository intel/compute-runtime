/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

namespace NEO {

enum PatchInfoAllocationType {
    Default = 0,
    KernelArg,
    GeneralStateHeap,
    DynamicStateHeap,
    IndirectObjectHeap,
    SurfaceStateHeap,
    InstructionHeap,
    TagAddress,
    TagValue,
    GUCStartMessage,
    ScratchSpace
};

struct PatchInfoData {
    uint64_t sourceAllocation;
    uint64_t sourceAllocationOffset;
    PatchInfoAllocationType sourceType;
    uint64_t targetAllocation;
    uint64_t targetAllocationOffset;
    PatchInfoAllocationType targetType;
    uint32_t patchAddressSize;

    PatchInfoData(uint64_t sourceAllocation,
                  uint64_t sourceAllocationOffset,
                  PatchInfoAllocationType sourceType,
                  uint64_t targetAllocation,
                  uint64_t targetAllocationOffset,
                  PatchInfoAllocationType targetType,
                  uint32_t patchAddressSize)
        : sourceAllocation(sourceAllocation),
          sourceAllocationOffset(sourceAllocationOffset),
          sourceType(sourceType),
          targetAllocation(targetAllocation),
          targetAllocationOffset(targetAllocationOffset),
          targetType(targetType),
          patchAddressSize(patchAddressSize) {
    }

    PatchInfoData(uint64_t sourceAllocation,
                  uint64_t sourceAllocationOffset,
                  PatchInfoAllocationType sourceType,
                  uint64_t targetAllocation,
                  uint64_t targetAllocationOffset,
                  PatchInfoAllocationType targetType)
        : sourceAllocation(sourceAllocation),
          sourceAllocationOffset(sourceAllocationOffset),
          sourceType(sourceType),
          targetAllocation(targetAllocation),
          targetAllocationOffset(targetAllocationOffset),
          targetType(targetType),
          patchAddressSize(sizeof(void *)) {
    }

    bool requiresIndirectPatching() {
        return (targetType != PatchInfoAllocationType::Default && targetType != PatchInfoAllocationType::GUCStartMessage);
    }
};

struct CommandChunk {
    uint64_t baseAddressCpu = 0;
    uint64_t baseAddressGpu = 0;
    uint64_t startOffset = 0;
    uint64_t endOffset = 0;
    uint64_t batchBufferStartLocation = 0;
    uint64_t batchBufferStartAddress = 0;
};
} // namespace NEO
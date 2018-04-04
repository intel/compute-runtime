/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <stdint.h>

namespace OCLRT {

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
} // namespace OCLRT
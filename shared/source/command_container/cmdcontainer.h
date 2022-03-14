/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace NEO {
class Device;
class GraphicsAllocation;
class LinearStream;
class AllocationsList;
class IndirectHeap;

using ResidencyContainer = std::vector<GraphicsAllocation *>;
using CmdBufferContainer = std::vector<GraphicsAllocation *>;
using HeapContainer = std::vector<GraphicsAllocation *>;
using HeapType = IndirectHeapType;

enum class ErrorCode {
    SUCCESS = 0,
    OUT_OF_DEVICE_MEMORY = 1
};

class CommandContainer : public NonCopyableOrMovableClass {
  public:
    static constexpr size_t defaultListCmdBufferSize = MemoryConstants::kiloByte * 256;
    static constexpr size_t cmdBufferReservedSize = MemoryConstants::cacheLineSize +
                                                    CSRequirements::csOverfetchSize;
    static constexpr size_t totalCmdBufferSize = defaultListCmdBufferSize + cmdBufferReservedSize;

    CommandContainer();

    CommandContainer(uint32_t maxNumAggregatedIdds);

    CmdBufferContainer &getCmdBufferAllocations() { return cmdBufferAllocations; }

    ResidencyContainer &getResidencyContainer() { return residencyContainer; }

    std::vector<GraphicsAllocation *> &getDeallocationContainer() { return deallocationContainer; }

    void addToResidencyContainer(GraphicsAllocation *alloc);
    void removeDuplicatesFromResidencyContainer();

    LinearStream *getCommandStream() { return commandStream.get(); }

    IndirectHeap *getIndirectHeap(HeapType heapType);

    HeapHelper *getHeapHelper() { return heapHelper.get(); }

    GraphicsAllocation *getIndirectHeapAllocation(HeapType heapType) { return allocationIndirectHeaps[heapType]; }

    void setIndirectHeapAllocation(HeapType heapType, GraphicsAllocation *allocation) { allocationIndirectHeaps[heapType] = allocation; }

    uint64_t getInstructionHeapBaseAddress() const { return instructionHeapBaseAddress; }

    uint64_t getIndirectObjectHeapBaseAddress() const { return indirectObjectHeapBaseAddress; }

    void *getHeapSpaceAllowGrow(HeapType heapType, size_t size);

    ErrorCode initialize(Device *device, AllocationsList *reusableAllocationList);

    void prepareBindfulSsh();

    virtual ~CommandContainer();

    uint32_t slmSize = std::numeric_limits<uint32_t>::max();
    uint32_t nextIddInBlock = 0;
    bool lastPipelineSelectModeRequired = false;
    bool lastSentUseGlobalAtomics = false;

    Device *getDevice() const { return device; }

    IndirectHeap *getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment);
    void allocateNextCommandBuffer();
    void closeAndAllocateNextCommandBuffer();

    void handleCmdBufferAllocations(size_t startIndex);
    GraphicsAllocation *obtainNextCommandBufferAllocation();

    void reset();

    bool isHeapDirty(HeapType heapType) const { return (dirtyHeaps & (1u << heapType)); }
    bool isAnyHeapDirty() const { return dirtyHeaps != 0; }
    void setHeapDirty(HeapType heapType) { dirtyHeaps |= (1u << heapType); }
    void setDirtyStateForAllHeaps(bool dirty) { dirtyHeaps = dirty ? std::numeric_limits<uint32_t>::max() : 0; }
    void setIddBlock(void *iddBlock) { this->iddBlock = iddBlock; }
    void *getIddBlock() { return iddBlock; }
    uint32_t getNumIddPerBlock() const { return numIddsPerBlock; }
    void setReservedSshSize(size_t reserveSize) {
        reservedSshSize = reserveSize;
    }
    HeapContainer sshAllocations;

    bool getFlushTaskUsedForImmediate() const { return isFlushTaskUsedForImmediate; }
    void setFlushTaskUsedForImmediate(bool flushTaskUsedForImmediate) { isFlushTaskUsedForImmediate = flushTaskUsedForImmediate; }

  protected:
    void *iddBlock = nullptr;
    Device *device = nullptr;
    AllocationsList *reusableAllocationList = nullptr;
    std::unique_ptr<HeapHelper> heapHelper;

    CmdBufferContainer cmdBufferAllocations;
    GraphicsAllocation *allocationIndirectHeaps[HeapType::NUM_TYPES] = {};
    uint64_t instructionHeapBaseAddress = 0u;
    uint64_t indirectObjectHeapBaseAddress = 0u;
    uint32_t dirtyHeaps = std::numeric_limits<uint32_t>::max();
    uint32_t numIddsPerBlock = 64;
    size_t reservedSshSize = 0;

    std::unique_ptr<LinearStream> commandStream;
    std::unique_ptr<IndirectHeap> indirectHeaps[HeapType::NUM_TYPES];
    ResidencyContainer residencyContainer;
    std::vector<GraphicsAllocation *> deallocationContainer;

    bool isFlushTaskUsedForImmediate = false;
};

} // namespace NEO

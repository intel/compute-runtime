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
class CommandStreamReceiver;
class Device;
class GraphicsAllocation;
class LinearStream;
class AllocationsList;
class IndirectHeap;

using ResidencyContainer = std::vector<GraphicsAllocation *>;
using CmdBufferContainer = std::vector<GraphicsAllocation *>;
using HeapContainer = std::vector<GraphicsAllocation *>;
using HeapType = IndirectHeapType;

class CommandContainer : public NonCopyableOrMovableClass {
  public:
    enum class ErrorCode {
        SUCCESS = 0,
        OUT_OF_DEVICE_MEMORY = 1
    };

    static constexpr size_t defaultListCmdBufferSize = 1u * MemoryConstants ::megaByte;
    static constexpr size_t cmdBufferReservedSize = MemoryConstants::cacheLineSize +
                                                    CSRequirements::csOverfetchSize;
    static constexpr size_t totalCmdBufferSize = defaultListCmdBufferSize + cmdBufferReservedSize;
    static constexpr size_t startingResidencyContainerSize = 128;

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

    ErrorCode initialize(Device *device, AllocationsList *reusableAllocationList, bool requireHeaps);

    void prepareBindfulSsh();

    virtual ~CommandContainer();

    Device *getDevice() const { return device; }

    MOCKABLE_VIRTUAL IndirectHeap *getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment);
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
    void setNumIddPerBlock(uint32_t value) { numIddsPerBlock = value; }
    void setReservedSshSize(size_t reserveSize) {
        reservedSshSize = reserveSize;
    }

    bool getFlushTaskUsedForImmediate() const { return isFlushTaskUsedForImmediate; }
    void setFlushTaskUsedForImmediate(bool flushTaskUsedForImmediate) { isFlushTaskUsedForImmediate = flushTaskUsedForImmediate; }
    void setImmediateCmdListCsr(CommandStreamReceiver *newValue) {
        this->immediateCmdListCsr = newValue;
    }
    void enableHeapSharing() { heapSharingEnabled = true; }
    bool immediateCmdListSharedHeap(HeapType heapType) {
        return (heapSharingEnabled && (heapType == HeapType::DYNAMIC_STATE || heapType == HeapType::SURFACE_STATE));
    }
    void ensureHeapSizePrepared(size_t sshRequiredSize, size_t dshRequiredSize);

    GraphicsAllocation *reuseExistingCmdBuffer();
    GraphicsAllocation *allocateCommandBuffer();
    void setCmdBuffer(GraphicsAllocation *cmdBuffer);
    void addCurrentCommandBufferToReusableAllocationList();

    void fillReusableAllocationLists();
    void storeAllocationAndFlushTagUpdate(GraphicsAllocation *allocation);

    HeapContainer sshAllocations;
    uint64_t currentLinearStreamStartOffset = 0u;
    uint32_t slmSize = std::numeric_limits<uint32_t>::max();
    uint32_t nextIddInBlock = 0;
    bool lastPipelineSelectModeRequired = false;
    bool lastSentUseGlobalAtomics = false;
    bool systolicModeSupport = false;

  protected:
    size_t getTotalCmdBufferSize();
    void createAndAssignNewHeap(HeapType heapType, size_t size);
    GraphicsAllocation *allocationIndirectHeaps[HeapType::NUM_TYPES] = {};
    std::unique_ptr<IndirectHeap> indirectHeaps[HeapType::NUM_TYPES];

    CmdBufferContainer cmdBufferAllocations;
    ResidencyContainer residencyContainer;
    std::vector<GraphicsAllocation *> deallocationContainer;

    std::unique_ptr<HeapHelper> heapHelper;
    std::unique_ptr<LinearStream> commandStream;

    uint64_t instructionHeapBaseAddress = 0u;
    uint64_t indirectObjectHeapBaseAddress = 0u;

    void *iddBlock = nullptr;
    Device *device = nullptr;
    AllocationsList *reusableAllocationList = nullptr;
    size_t reservedSshSize = 0;
    CommandStreamReceiver *immediateCmdListCsr = nullptr;
    IndirectHeap *sharedSshCsrHeap = nullptr;
    IndirectHeap *sharedDshCsrHeap = nullptr;

    uint32_t dirtyHeaps = std::numeric_limits<uint32_t>::max();
    uint32_t numIddsPerBlock = 64;

    bool isFlushTaskUsedForImmediate = false;
    bool isHandleFenceCompletionRequired = false;
    bool heapSharingEnabled = false;
};

} // namespace NEO

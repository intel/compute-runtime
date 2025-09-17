/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/source/os_interface/os_context.h"

#include "aubstream/allocation_params.h"

namespace NEO {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedHw : public CommandStreamReceiverSimulatedCommonHw<GfxFamily> {
  protected:
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::CommandStreamReceiverSimulatedCommonHw;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::osContext;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getDeviceIndex;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::aubManager;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::hardwareContextController;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::writeMemory;
    using CommandStreamReceiverSimulatedCommonHw<GfxFamily>::downloadAllocations;

  public:
    uint32_t getMemoryBank(GraphicsAllocation *allocation) const {
        if (aubManager) {
            return static_cast<uint32_t>(getMemoryBanksBitfield(allocation).to_ulong());
        }

        uint32_t deviceIndexChosen = allocation->storageInfo.memoryBanks.any()
                                         ? getDeviceIndexFromStorageInfo(allocation->storageInfo)
                                         : getDeviceIndex();

        if (allocation->getMemoryPool() == MemoryPool::localMemory) {
            return MemoryBanks::getBankForLocalMemory(deviceIndexChosen);
        }
        return MemoryBanks::getBank(deviceIndexChosen);
    }

    static uint32_t getDeviceIndexFromStorageInfo(StorageInfo storageInfo) {
        uint32_t deviceIndex = 0;
        while (!storageInfo.memoryBanks.test(0)) {
            storageInfo.memoryBanks >>= 1;
            deviceIndex++;
        }
        return deviceIndex;
    }

    DeviceBitfield getMemoryBanksBitfield(GraphicsAllocation *allocation) const {
        if (allocation->getMemoryPool() == MemoryPool::localMemory) {
            if (allocation->storageInfo.memoryBanks.any()) {
                if (allocation->storageInfo.cloningOfPageTables || this->isMultiOsContextCapable()) {
                    return allocation->storageInfo.memoryBanks;
                }
            }
            return this->osContext->getDeviceBitfield();
        }
        return {};
    }

    int getAddressSpace(int hint) {
        bool traceLocalAllowed = false;
        switch (hint) {
        case AubMemDump::DataTypeHintValues::TraceLogicalRingContextRcs:
        case AubMemDump::DataTypeHintValues::TraceLogicalRingContextCcs:
        case AubMemDump::DataTypeHintValues::TraceLogicalRingContextBcs:
        case AubMemDump::DataTypeHintValues::TraceLogicalRingContextVcs:
        case AubMemDump::DataTypeHintValues::TraceLogicalRingContextVecs:
        case AubMemDump::DataTypeHintValues::TraceCommandBuffer:
            traceLocalAllowed = true;
            break;
        default:
            break;
        }
        if ((traceLocalAllowed && this->localMemoryEnabled) || debugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
            return AubMemDump::AddressSpaceValues::TraceLocal;
        }
        return AubMemDump::AddressSpaceValues::TraceNonlocal;
    }
    PhysicalAddressAllocator *createPhysicalAddressAllocator(const HardwareInfo *hwInfo, const ReleaseHelper *releaseHelper) {
        const auto bankSize = AubHelper::getPerTileLocalMemorySize(hwInfo, releaseHelper);
        const auto devicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
        return new PhysicalAddressAllocatorHw<GfxFamily>(bankSize, devicesCount);
    }
    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        uint64_t gpuAddress;
        void *cpuAddress;
        size_t allocSize;
        this->getParametersForMemory(graphicsAllocation, gpuAddress, cpuAddress, allocSize);
        int hint = graphicsAllocation.getAllocationType() == AllocationType::commandBuffer
                       ? AubMemDump::DataTypeHintValues::TraceBatchBuffer
                       : AubMemDump::DataTypeHintValues::TraceNotype;

        if (isChunkCopy) {
            gpuAddress += gpuVaChunkOffset;
            cpuAddress = ptrOffset(cpuAddress, static_cast<uintptr_t>(gpuVaChunkOffset));
            allocSize = chunkSize;
        }

        aub_stream::AllocationParams allocationParams(gpuAddress, cpuAddress, allocSize, this->getMemoryBank(&graphicsAllocation),
                                                      hint, graphicsAllocation.getUsedPageSize());

        auto gmm = graphicsAllocation.getDefaultGmm();

        if (gmm) {
            allocationParams.additionalParams.compressionEnabled = gmm->isCompressionEnabled();
            allocationParams.additionalParams.uncached = CacheSettingsHelper::isUncachedType(gmm->resourceParams.Usage);
        }

        if (graphicsAllocation.storageInfo.cloningOfPageTables || !graphicsAllocation.isAllocatedInLocalMemoryPool()) {
            UNRECOVERABLE_IF(nullptr == aubManager);
            aubManager->writeMemory2(allocationParams);
        } else {
            UNRECOVERABLE_IF(nullptr == hardwareContextController);
            hardwareContextController->writeMemory(allocationParams);
        }
    }

    void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation) override {
        auto bank = getMemoryBank(&graphicsAllocation);
        if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
            bank = GraphicsAllocation::defaultBank;
        }

        graphicsAllocation.setAubWritable(writable, bank);
    }

    bool isAubWritable(GraphicsAllocation &graphicsAllocation) const override {
        auto bank = getMemoryBank(&graphicsAllocation);
        if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
            bank = GraphicsAllocation::defaultBank;
        }
        return graphicsAllocation.isAubWritable(bank);
    }

    void setTbxWritable(bool writable, GraphicsAllocation &graphicsAllocation) override {
        auto bank = getMemoryBank(&graphicsAllocation);
        if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
            bank = GraphicsAllocation::defaultBank;
        }
        graphicsAllocation.setTbxWritable(writable, bank);
    }

    bool isTbxWritable(GraphicsAllocation &graphicsAllocation) const override {
        auto bank = getMemoryBank(&graphicsAllocation);
        if (bank == 0u || graphicsAllocation.storageInfo.cloningOfPageTables) {
            bank = GraphicsAllocation::defaultBank;
        }
        return graphicsAllocation.isTbxWritable(bank);
    }
};
} // namespace NEO

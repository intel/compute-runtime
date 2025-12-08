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
#include "shared/source/memory_manager/physical_address_allocator.h"
#include "shared/source/os_interface/os_context.h"

#include "aubstream/allocation_params.h"
#include "aubstream/hint_values.h"

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
                       ? aub_stream::DataTypeHintValues::TraceBatchBuffer
                       : aub_stream::DataTypeHintValues::TraceNotype;

        if (isChunkCopy) {
            gpuAddress += gpuVaChunkOffset;
            cpuAddress = ptrOffset(cpuAddress, static_cast<uintptr_t>(gpuVaChunkOffset));
            allocSize = chunkSize;
        }

        std::unique_ptr<unsigned char[]> memoryCopy;
        if (graphicsAllocation.isLocked() && debugManager.flags.CopyLockedMemoryBeforeWrite.get()) {
            memoryCopy = std::make_unique_for_overwrite<unsigned char[]>(allocSize);
            memcpy_s(memoryCopy.get(), allocSize, cpuAddress, allocSize);
            cpuAddress = memoryCopy.get();
        }

        aub_stream::AllocationParams allocationParams(gpuAddress, cpuAddress, allocSize, this->getMemoryBank(&graphicsAllocation),
                                                      hint, graphicsAllocation.getUsedPageSize());

        auto gmm = graphicsAllocation.getDefaultGmm();

        if (gmm) {
            allocationParams.additionalParams.compressionEnabled = gmm->isCompressionEnabled();
            allocationParams.additionalParams.uncached = CacheSettingsHelper::isUncachedType(gmm->getResourceUsageType());
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

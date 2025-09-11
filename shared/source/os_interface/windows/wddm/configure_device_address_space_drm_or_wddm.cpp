/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_callbacks.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/gfx_escape_wrapper.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_temp_storage.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "gmm_memory.h"

namespace NEO {

struct GfxSynchPartitionInfoEscapeHeader {
    GFX_ESCAPE_HEADER_T base;
    uint32_t action;
    uint32_t padding;

    static constexpr uint32_t gmmEscapeCode = 2; // GFX_ESCAPE_GMM_CONTROL
    static constexpr uint32_t actionGet = 28;    // // GMM_ESCAPE_ACTION_GET_PROCESS_GFX_PARTITION
    static constexpr uint32_t actionSet = 29;    // // GMM_ESCAPE_ACTION_SET_PROCESS_GFX_PARTITION
    static constexpr uint32_t escapeDataTailPaddingInBytes = sizeof(uint32_t) * 2;
};

bool synchronizePartitionLayoutWithinProcess(Wddm &wddm, GMM_GFX_PARTITIONING &partitionLayout, uint32_t action) {
    UmKmDataTempStorage<GMM_GFX_PARTITIONING> escapeDataStorage;
    auto translator = wddm.getHwDeviceId()->getUmKmDataTranslator();
    size_t partitioningStructInternalRepresentationSize = translator->getSizeForGmmGfxPartitioningInternalRepresentation();
    size_t escapeDataSize = sizeof(GfxSynchPartitionInfoEscapeHeader) + partitioningStructInternalRepresentationSize + GfxSynchPartitionInfoEscapeHeader::escapeDataTailPaddingInBytes;
    escapeDataStorage.resize(escapeDataSize);

    auto escapeHeader = static_cast<GfxSynchPartitionInfoEscapeHeader *>(escapeDataStorage.data());
    escapeHeader->base.EscapeCode = GfxSynchPartitionInfoEscapeHeader::gmmEscapeCode;
    escapeHeader->base.Size = static_cast<uint32_t>(escapeDataStorage.size() - sizeof(escapeHeader->base));
    escapeHeader->action = action;
    if (action == GfxSynchPartitionInfoEscapeHeader::actionSet) {
        auto translatedSuccesfully = translator->translateGmmGfxPartitioningToInternalRepresentation(escapeHeader + 1,
                                                                                                     escapeDataStorage.size() - sizeof(escapeHeader),
                                                                                                     partitionLayout);
        if (false == translatedSuccesfully) {
            DEBUG_BREAK_IF(true);
            return false;
        }
    }

    D3DKMT_ESCAPE d3dkmtEscapeInfo = {};
    d3dkmtEscapeInfo.hAdapter = wddm.getAdapter();
    d3dkmtEscapeInfo.hDevice = wddm.getDeviceHandle();
    d3dkmtEscapeInfo.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    d3dkmtEscapeInfo.pPrivateDriverData = escapeDataStorage.data();
    d3dkmtEscapeInfo.PrivateDriverDataSize = static_cast<uint32_t>(escapeDataStorage.size());

    NTSTATUS status = wddm.getGdi()->escape(&d3dkmtEscapeInfo);
    if (status != STATUS_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    auto translatedSuccesfully = translator->translateGmmGfxPartitioningFromInternalRepresentation(partitionLayout, static_cast<void *>(escapeHeader + 1),
                                                                                                   partitioningStructInternalRepresentationSize);
    DEBUG_BREAK_IF(false == translatedSuccesfully);
    return translatedSuccesfully;
}

bool readPartitionLayoutWithinProcess(Wddm &wddm, GMM_GFX_PARTITIONING &partitionLayout) {
    return synchronizePartitionLayoutWithinProcess(wddm, partitionLayout, GfxSynchPartitionInfoEscapeHeader::actionGet);
}

bool tryWritePartitionLayoutWithinProcess(Wddm &wddm, GMM_GFX_PARTITIONING &partitionLayout) {
    return synchronizePartitionLayoutWithinProcess(wddm, partitionLayout, GfxSynchPartitionInfoEscapeHeader::actionSet);
}

bool adjustGfxPartitionLayout(GMM_GFX_PARTITIONING &partitionLayout, uint64_t gpuAddressSpace, uintptr_t minAllowedAddress, Wddm &wddm, PRODUCT_FAMILY productFamily) {
    bool requiresRepartitioning = (gpuAddressSpace == maxNBitValue(47)) && wddm.getFeatureTable().flags.ftrCCSRing;
    auto &productHelper = wddm.getRootDeviceEnvironment().getHelper<ProductHelper>();
    requiresRepartitioning |= productHelper.overrideGfxPartitionLayoutForWsl();
    if (false == requiresRepartitioning) {
        return true;
    }

    auto usmLimit = partitionLayout.SVM.Limit;

    if (false == readPartitionLayoutWithinProcess(wddm, partitionLayout)) {
        return false;
    } else if (partitionLayout.Standard64KB.Limit != 0) {
        // already partitioned
        return true;
    }

    partitionLayout.SVM.Base = minAllowedAddress;
    partitionLayout.SVM.Limit = usmLimit;

    constexpr uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte; // 4GB
    const auto cpuAddressRangeSizeToReserve = getSizeToReserve();

    void *reservedRangeBase = nullptr;
    if (false == wddm.reserveValidAddressRange(cpuAddressRangeSizeToReserve + 2 * MemoryConstants::megaByte, reservedRangeBase)) {
        DEBUG_BREAK_IF(true);
        return false;
    }
    auto reservedRangeBaseAligned = alignUp(reservedRangeBase, 2 * MemoryConstants::megaByte);

    auto gfxBase = reinterpret_cast<uint64_t>(reservedRangeBaseAligned);
    auto gfxTop = gfxBase + cpuAddressRangeSizeToReserve;

    for (auto &heap : partitionLayout.Heap32) {
        heap.Base = gfxBase;
        heap.Limit = heap.Base + gfxHeap32Size;
        gfxBase += gfxHeap32Size;
    }

    constexpr uint32_t numStandardHeaps = 2; // Standard and Standard64
    constexpr uint64_t maxStandardHeapGranularity = GfxPartition::heapGranularity;
    uint64_t maxStandardHeapSize = alignDown((gfxTop - gfxBase) / numStandardHeaps, maxStandardHeapGranularity);

    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    partitionLayout.Standard.Base = gfxBase;
    partitionLayout.Standard.Limit = partitionLayout.Standard.Base + maxStandardHeapSize;

    gfxBase += maxStandardHeapSize;
    partitionLayout.Standard64KB.Base = gfxBase;
    partitionLayout.Standard64KB.Limit = partitionLayout.Standard64KB.Base + maxStandardHeapSize;

    if (false == tryWritePartitionLayoutWithinProcess(wddm, partitionLayout)) {
        wddm.releaseReservedAddress(reservedRangeBase);
        return false;
    }

    if (reinterpret_cast<uintptr_t>(reservedRangeBaseAligned) != partitionLayout.Heap32[0].Base) {
        // different layout acquired from KMD
        wddm.releaseReservedAddress(reservedRangeBase);
    }

    return true;
}

bool ensureGpuAddressRangeIsReserved(uint64_t address, size_t size, D3DKMT_HANDLE adapter, Gdi &gdi) {
    D3DDDI_RESERVEGPUVIRTUALADDRESS rangeDesc = {};
    rangeDesc.BaseAddress = address;
    rangeDesc.Size = size;
    rangeDesc.hAdapter = adapter;
    NTSTATUS status = gdi.reserveGpuVirtualAddress(&rangeDesc);
    if (status != STATUS_SUCCESS) {
        // make sure address range is already reserved
        rangeDesc.BaseAddress = 0;
        rangeDesc.MinimumAddress = alignDown(address, MemoryConstants::pageSize64k);
        rangeDesc.MaximumAddress = alignUp(address + size, MemoryConstants::pageSize64k);
        rangeDesc.Size = MemoryConstants::pageSize64k;
        status = gdi.reserveGpuVirtualAddress(&rangeDesc);
        if (status == STATUS_SUCCESS) {
            DEBUG_BREAK_IF(true);
            return false;
        }
    }
    return true;
}

bool Wddm::configureDeviceAddressSpace() {
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
    GMM_DEVICE_INFO deviceInfo{};
    deviceInfo.pDeviceCb = &deviceCallbacks;
    if (!gmmMemory->setDeviceInfo(&deviceInfo)) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    auto productFamily = gfxPlatform->eProductFamily;
    if (!hardwareInfoTable[productFamily]) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    auto gpuAddressSpace = hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace;
    maximumApplicationAddress = MemoryConstants::max64BitAppAddress;

    uintptr_t usmBase = std::max(gfxPartition.SVM.Base, windowsMinAddress);
    this->minAddress = usmBase;

    uint64_t maxUsmSize = 0;
    if (gpuAddressSpace >= maximumApplicationAddress) {
        maxUsmSize = maximumApplicationAddress + 1u;
    }

    if (maxUsmSize > 0) {
        if (false == adjustGfxPartitionLayout(this->gfxPartition, gpuAddressSpace, usmBase, *this, gfxPlatform->eProductFamily)) {
            return false;
        }
        if (gfxPartition.Standard64KB.Limit <= maxUsmSize) {
            uintptr_t usmLowPartMax = gfxPartition.Heap32[0].Base;
            for (const auto &usmMaxAddr : {gfxPartition.Standard.Base, gfxPartition.Standard64KB.Base, gfxPartition.TR.Base}) {
                if (usmMaxAddr != 0) {
                    usmLowPartMax = std::min(usmLowPartMax, usmMaxAddr);
                }
            }
            // reserved cpu address range splits USM into 2 partitions
            struct {
                uint64_t minAddr;
                uint64_t maxAddr;
            } usmRanges[] = {{usmBase, usmLowPartMax},
                             {alignUp(gfxPartition.Standard64KB.Limit, MemoryConstants::pageSize64k), maxUsmSize}};

            bool usmGpuRangeIsReserved = true;
            for (const auto &usmRange : usmRanges) {
                usmGpuRangeIsReserved = usmGpuRangeIsReserved && ensureGpuAddressRangeIsReserved(usmRange.minAddr, usmRange.maxAddr - usmRange.minAddr, this->getAdapter(), *this->getGdi());
            }
            if (false == usmGpuRangeIsReserved) {
                return false;
            }
        } else {
            auto usmGpuRangeIsReserved = ensureGpuAddressRangeIsReserved(usmBase, maxUsmSize - usmBase, this->getAdapter(), *this->getGdi());
            if (false == usmGpuRangeIsReserved) {
                return false;
            }
        }
    }

    uintptr_t addr = 0;
    auto ret = gmmMemory->configureDevice(getAdapter(), device, getGdi()->escape, maxUsmSize, featureTable->flags.ftrL3IACoherency, addr, false);
    return ret;
}

} // namespace NEO

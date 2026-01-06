/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_helper.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

namespace BlitHelperFunctions {
BlitMemoryToAllocationFunc blitMemoryToAllocation = BlitHelper::blitMemoryToAllocation;
BlitMemsetAllocationFunc blitMemsetAllocation = BlitHelper::blitMemsetAllocation;
} // namespace BlitHelperFunctions

BlitOperationResult BlitHelper::blitMemoryToAllocation(const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                       const Vec3<size_t> &size) {
    auto memoryBanks = memory->storageInfo.getMemoryBanks();
    return blitMemoryToAllocationBanks(device, memory, offset, hostPtr, size, memoryBanks);
}

BlitOperationResult BlitHelper::blitMemoryToAllocationBanks(const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                            const Vec3<size_t> &size, DeviceBitfield memoryBanks) {
    const auto &hwInfo = device.getHardwareInfo();
    if (!hwInfo.capabilityTable.blitterOperationsSupported) {
        return BlitOperationResult::unsupported;
    }
    auto &gfxCoreHelper = device.getGfxCoreHelper();

    UNRECOVERABLE_IF(memoryBanks.none());

    auto pRootDevice = device.getRootDevice();

    for (uint8_t tileId = 0u; tileId < 4u; tileId++) {
        if (!memoryBanks.test(tileId)) {
            continue;
        }

        UNRECOVERABLE_IF(!pRootDevice->getDeviceBitfield().test(tileId));
        auto pDeviceForBlit = pRootDevice->getNearestGenericSubDevice(tileId);
        auto &selectorCopyEngine = pDeviceForBlit->getSelectorCopyEngine();
        auto deviceBitfield = pDeviceForBlit->getDeviceBitfield();
        auto internalUsage = true;
        auto bcsEngineType = EngineHelpers::getBcsEngineType(pDeviceForBlit->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine, internalUsage);
        auto bcsEngineUsage = gfxCoreHelper.preferInternalBcsEngine() ? EngineUsage::internal : EngineUsage::regular;
        auto bcsEngine = pDeviceForBlit->tryGetEngine(bcsEngineType, bcsEngineUsage);
        if (!bcsEngine) {
            return BlitOperationResult::unsupported;
        }

        bcsEngine->commandStreamReceiver->initializeResources(false, device.getPreemptionMode());
        bcsEngine->commandStreamReceiver->initDirectSubmission();
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(
            BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                            *bcsEngine->commandStreamReceiver, memory, nullptr,
                                                            hostPtr,
                                                            (memory->getGpuAddress() + offset),
                                                            0, 0, 0, size, 0, 0, 0, 0));

        const auto newTaskCount = bcsEngine->commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, *pDeviceForBlit);
        if (newTaskCount == CompletionStamp::gpuHang) {
            return BlitOperationResult::gpuHang;
        }
    }

    return BlitOperationResult::success;
}

BlitOperationResult BlitHelper::blitMemsetAllocation(const Device &device, GraphicsAllocation *memory, size_t offset, int value, size_t size) {
    auto memoryBanks = memory->storageInfo.getMemoryBanks();
    return blitMemsetAllocationBanks(device, memory, offset, value, size, memoryBanks);
}

BlitOperationResult BlitHelper::blitMemsetAllocationBanks(const Device &device, GraphicsAllocation *memory, size_t offset, int value,
                                                          size_t size, DeviceBitfield memoryBanks) {
    const auto &hwInfo = device.getHardwareInfo();
    if (!hwInfo.capabilityTable.blitterOperationsSupported) {
        return BlitOperationResult::unsupported;
    }
    auto &gfxCoreHelper = device.getGfxCoreHelper();

    UNRECOVERABLE_IF(memoryBanks.none());

    auto pRootDevice = device.getRootDevice();

    uint8_t byteValue = static_cast<uint8_t>(value);
    uint32_t patternToCommand[4] = {};
    memcpy_s(patternToCommand, sizeof(patternToCommand), &byteValue, 1);

    for (uint8_t tileId = 0u; tileId < 4u; tileId++) {
        if (!memoryBanks.test(tileId)) {
            continue;
        }

        UNRECOVERABLE_IF(!pRootDevice->getDeviceBitfield().test(tileId));
        auto pDeviceForBlit = pRootDevice->getNearestGenericSubDevice(tileId);
        auto &selectorCopyEngine = pDeviceForBlit->getSelectorCopyEngine();
        auto deviceBitfield = pDeviceForBlit->getDeviceBitfield();
        auto internalUsage = true;
        auto bcsEngineType = EngineHelpers::getBcsEngineType(pDeviceForBlit->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine, internalUsage);
        auto bcsEngineUsage = gfxCoreHelper.preferInternalBcsEngine() ? EngineUsage::internal : EngineUsage::regular;
        auto bcsEngine = pDeviceForBlit->tryGetEngine(bcsEngineType, bcsEngineUsage);
        if (!bcsEngine) {
            return BlitOperationResult::unsupported;
        }

        bcsEngine->commandStreamReceiver->initializeResources(false, device.getPreemptionMode());
        bcsEngine->commandStreamReceiver->initDirectSubmission();

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(
            BlitProperties::constructPropertiesForMemoryFill(
                memory, 0, size, patternToCommand, 1, offset));

        const auto newTaskCount = bcsEngine->commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, *pDeviceForBlit);
        if (newTaskCount == CompletionStamp::gpuHang) {
            return BlitOperationResult::gpuHang;
        }
    }

    return BlitOperationResult::success;
}

} // namespace NEO

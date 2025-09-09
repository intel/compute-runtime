/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/memory_manager/migration_controller.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/migration_sync_data.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {
void MigrationController::handleMigration(Context &context, CommandStreamReceiver &targetCsr, MemObj *memObj) {
    auto memoryManager = targetCsr.getMemoryManager();
    auto targetRootDeviceIndex = targetCsr.getRootDeviceIndex();
    auto migrationSyncData = memObj->getMultiGraphicsAllocation().getMigrationSyncData();
    if (migrationSyncData->getCurrentLocation() != targetRootDeviceIndex) {
        if (!migrationSyncData->isUsedByTheSameContext(targetCsr.getTagAddress())) {
            migrationSyncData->waitOnCpu();
        }
        migrateMemory(context, *memoryManager, memObj, targetRootDeviceIndex);
    }
    migrationSyncData->signalUsage(targetCsr.getTagAddress(), targetCsr.peekTaskCount() + 1);
}

void MigrationController::migrateMemory(Context &context, MemoryManager &memoryManager, MemObj *memObj, uint32_t targetRootDeviceIndex) {
    auto &multiGraphicsAllocation = memObj->getMultiGraphicsAllocation();
    auto migrationSyncData = multiGraphicsAllocation.getMigrationSyncData();
    if (migrationSyncData->isMigrationInProgress()) {
        return;
    }

    auto sourceRootDeviceIndex = migrationSyncData->getCurrentLocation();
    if (sourceRootDeviceIndex == std::numeric_limits<uint32_t>::max()) {
        migrationSyncData->setCurrentLocation(targetRootDeviceIndex);
        return;
    }

    migrationSyncData->startMigration();

    auto srcMemory = multiGraphicsAllocation.getGraphicsAllocation(sourceRootDeviceIndex);
    auto dstMemory = multiGraphicsAllocation.getGraphicsAllocation(targetRootDeviceIndex);

    auto size = srcMemory->getUnderlyingBufferSize();
    auto hostPtr = migrationSyncData->getHostPtr();

    if (srcMemory->isAllocationLockable()) {
        auto srcLockPtr = memoryManager.lockResource(srcMemory);
        memcpy_s(hostPtr, size, srcLockPtr, size);
        memoryManager.unlockResource(srcMemory);
    } else {

        auto srcCmdQ = context.getSpecialQueue(sourceRootDeviceIndex);
        if (srcMemory->getAllocationType() == AllocationType::image) {
            auto pImage = static_cast<Image *>(memObj);
            size_t origin[3] = {};
            size_t region[3] = {};
            pImage->fillImageRegion(region);

            srcCmdQ->enqueueReadImage(pImage, CL_TRUE, origin, region, pImage->getHostPtrRowPitch(), pImage->getHostPtrSlicePitch(), hostPtr, nullptr, 0, nullptr, nullptr);
        } else {
            auto pBuffer = static_cast<Buffer *>(memObj);
            srcCmdQ->enqueueReadBuffer(pBuffer, CL_TRUE, 0u, pBuffer->getSize(), hostPtr, nullptr, 0, nullptr, nullptr);
        }
        srcCmdQ->finish(false);
    }

    if (dstMemory->isAllocationLockable()) {
        auto dstLockPtr = memoryManager.lockResource(dstMemory);
        memcpy_s(dstLockPtr, size, hostPtr, size);
        memoryManager.unlockResource(dstMemory);
    } else {

        auto dstCmdQ = context.getSpecialQueue(targetRootDeviceIndex);
        if (dstMemory->getAllocationType() == AllocationType::image) {
            auto pImage = static_cast<Image *>(memObj);
            size_t origin[3] = {};
            size_t region[3] = {};
            pImage->fillImageRegion(region);

            dstCmdQ->enqueueWriteImage(pImage, CL_TRUE, origin, region, pImage->getHostPtrRowPitch(), pImage->getHostPtrSlicePitch(), hostPtr, nullptr, 0, nullptr, nullptr);
        } else {
            auto pBuffer = static_cast<Buffer *>(memObj);
            dstCmdQ->enqueueWriteBuffer(pBuffer, CL_TRUE, 0u, pBuffer->getSize(), hostPtr, nullptr, 0, nullptr, nullptr);
        }
        dstCmdQ->finish(false);
    }
    migrationSyncData->setCurrentLocation(targetRootDeviceIndex);
}
} // namespace NEO

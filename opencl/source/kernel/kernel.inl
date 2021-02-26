/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/program/printf_handler.h"

namespace NEO {

template <bool mockable>
void Kernel::patchReflectionSurface(DeviceQueue *devQueue, PrintfHandler *printfHandler) {

    void *reflectionSurface = kernelReflectionSurface->getUnderlyingBuffer();

    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());
    auto rootDeviceIndex = devQueue->getDevice().getRootDeviceIndex();
    auto &kernelInfo = *kernelInfos[rootDeviceIndex];

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

        uint64_t printfBufferOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t printfBufferPatchSize = 0U;
        const auto &printfSurface = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
        if (isValidOffset(printfSurface.stateless)) {
            printfBufferOffset = printfSurface.stateless;
            printfBufferPatchSize = printfSurface.pointerSize;
        }
        uint64_t printfGpuAddress = 0;

        uint64_t eventPoolOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t eventPoolSize = 0U;
        const auto &eventPoolSurfaceAddress = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress;
        if (isValidOffset(eventPoolSurfaceAddress.stateless)) {
            eventPoolOffset = eventPoolSurfaceAddress.stateless;
            eventPoolSize = eventPoolSurfaceAddress.pointerSize;
        }

        uint64_t defaultQueueOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t defaultQueueSize = 0U;
        const auto &defaultQueueSurface = pBlockInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress;
        if (isValidOffset(defaultQueueSurface.stateless)) {
            defaultQueueOffset = defaultQueueSurface.stateless;
            defaultQueueSize = defaultQueueSurface.pointerSize;
        }

        uint64_t deviceQueueOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t deviceQueueSize = 0;

        uint64_t privateSurfaceOffset = ReflectionSurfaceHelper::undefinedOffset;
        uint32_t privateSurfacePatchSize = 0;
        uint64_t privateSurfaceGpuAddress = 0;

        auto privateSurface = blockManager->getPrivateSurface(i);

        UNRECOVERABLE_IF(pBlockInfo->patchInfo.pAllocateStatelessPrivateSurface != nullptr && pBlockInfo->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize && privateSurface == nullptr);

        if (privateSurface) {
            privateSurfaceOffset = pBlockInfo->patchInfo.pAllocateStatelessPrivateSurface->DataParamOffset;
            privateSurfacePatchSize = pBlockInfo->patchInfo.pAllocateStatelessPrivateSurface->DataParamSize;
            privateSurfaceGpuAddress = privateSurface->getGpuAddressToPatch();
        }

        if (printfHandler) {
            GraphicsAllocation *printfSurface = printfHandler->getSurface();

            if (printfSurface)
                printfGpuAddress = printfSurface->getGpuAddress();
        }

        for (const auto &arg : pBlockInfo->kernelArgInfo) {
            if (arg.isDeviceQueue) {
                deviceQueueOffset = arg.kernelArgPatchInfoVector[0].crossthreadOffset;
                deviceQueueSize = arg.kernelArgPatchInfoVector[0].size;
                break;
            }
        }

        ReflectionSurfaceHelper::patchBlocksCurbe<mockable>(reflectionSurface, i,
                                                            defaultQueueOffset, defaultQueueSize, devQueue->getQueueBuffer()->getGpuAddress(),
                                                            eventPoolOffset, eventPoolSize, devQueue->getEventPoolBuffer()->getGpuAddress(),
                                                            deviceQueueOffset, deviceQueueSize, devQueue->getQueueBuffer()->getGpuAddress(),
                                                            printfBufferOffset, printfBufferPatchSize, printfGpuAddress,
                                                            privateSurfaceOffset, privateSurfacePatchSize, privateSurfaceGpuAddress);
    }

    ReflectionSurfaceHelper::setParentImageParams(reflectionSurface, this->kernelArguments, kernelInfo);
    ReflectionSurfaceHelper::setParentSamplerParams(reflectionSurface, this->kernelArguments, kernelInfo);
}
} // namespace NEO

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/kernel.h"
#include "runtime/program/printf_handler.h"

namespace NEO {

template <bool mockable>
void Kernel::patchReflectionSurface(DeviceQueue *devQueue, PrintfHandler *printfHandler) {

    void *reflectionSurface = kernelReflectionSurface->getUnderlyingBuffer();

    BlockKernelManager *blockManager = program->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

        // clang-format off
        uint64_t defaultQueueOffset = pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface ? 
            pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamOffset : ReflectionSurfaceHelper::undefinedOffset;
        uint64_t eventPoolOffset = pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface ? 
            pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamOffset : ReflectionSurfaceHelper::undefinedOffset;
        uint64_t deviceQueueOffset = ReflectionSurfaceHelper::undefinedOffset;

        uint32_t defaultQueueSize = pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface ? 
            pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamSize : 0;
        uint32_t eventPoolSize = pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface ? 
            pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamSize : 0;
        uint32_t deviceQueueSize = 0;

        uint64_t printfBufferOffset = pBlockInfo->patchInfo.pAllocateStatelessPrintfSurface ?
            pBlockInfo->patchInfo.pAllocateStatelessPrintfSurface->DataParamOffset : ReflectionSurfaceHelper::undefinedOffset;
        uint32_t printfBufferPatchSize = pBlockInfo->patchInfo.pAllocateStatelessPrintfSurface ?
            pBlockInfo->patchInfo.pAllocateStatelessPrintfSurface->DataParamSize : 0;
        uint64_t printfGpuAddress = 0;
        // clang-format on

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

    ReflectionSurfaceHelper::setParentImageParams(reflectionSurface, this->kernelArguments, this->kernelInfo);
    ReflectionSurfaceHelper::setParentSamplerParams(reflectionSurface, this->kernelArguments, this->kernelInfo);
}
} // namespace NEO

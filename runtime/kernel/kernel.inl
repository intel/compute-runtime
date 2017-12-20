/*
 * Copyright (c) 2017, Intel Corporation
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

namespace OCLRT {

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

        UNRECOVERABLE_IF(pBlockInfo->patchInfo.pAllocateStatelessPrintfSurface != nullptr && privateSurface == nullptr);

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

        if (pBlockInfo->kernelArgInfo.size() > 0) {
            for (uint32_t i = 0; i < pBlockInfo->kernelArgInfo.size(); i++) {
                if (pBlockInfo->kernelArgInfo[i].isDeviceQueue) {
                    deviceQueueOffset = pBlockInfo->kernelArgInfo[i].kernelArgPatchInfoVector[0].crossthreadOffset;
                    deviceQueueSize = pBlockInfo->kernelArgInfo[i].kernelArgPatchInfoVector[0].size;
                    break;
                }
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
} // namespace OCLRT

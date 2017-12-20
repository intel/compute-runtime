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

#include "runtime/program/printf_handler.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "runtime/kernel/kernel.inl"

namespace OCLRT {

MockKernel::BlockPatchValues MockKernel::ReflectionSurfaceHelperPublic::devQueue;
MockKernel::BlockPatchValues MockKernel::ReflectionSurfaceHelperPublic::defaultQueue;
MockKernel::BlockPatchValues MockKernel::ReflectionSurfaceHelperPublic::eventPool;
MockKernel::BlockPatchValues MockKernel::ReflectionSurfaceHelperPublic::printfBuffer;

template <>
void Kernel::ReflectionSurfaceHelper::patchBlocksCurbe<true>(void *reflectionSurface, uint32_t blockID,
                                                             uint64_t defaultDeviceQueueCurbeOffset, uint32_t patchSizeDefaultQueue, uint64_t defaultDeviceQueueGpuAddress,
                                                             uint64_t eventPoolCurbeOffset, uint32_t patchSizeEventPool, uint64_t eventPoolGpuAddress,
                                                             uint64_t deviceQueueCurbeOffset, uint32_t patchSizeDeviceQueue, uint64_t deviceQueueGpuAddress,
                                                             uint64_t printfBufferOffset, uint32_t patchSizePrintfBuffer, uint64_t printfBufferGpuAddress,
                                                             uint64_t privateSurfaceOffset, uint32_t privateSurfaceSize, uint64_t privateSurfaceGpuAddress) {

    MockKernel::ReflectionSurfaceHelperPublic::patchBlocksCurbeMock(reflectionSurface, blockID,
                                                                    defaultDeviceQueueCurbeOffset, patchSizeDefaultQueue, defaultDeviceQueueGpuAddress,
                                                                    eventPoolCurbeOffset, patchSizeEventPool, eventPoolGpuAddress,
                                                                    deviceQueueCurbeOffset, patchSizeDeviceQueue, deviceQueueGpuAddress,
                                                                    printfBufferOffset, patchSizePrintfBuffer, printfBufferGpuAddress);
}

template void Kernel::patchReflectionSurface<true>(DeviceQueue *, PrintfHandler *);

bool MockKernel::isPatched() const {
    return true;
}

} // namespace OCLRT

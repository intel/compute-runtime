/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "printf_handler.h"

#include "runtime/mem_obj/buffer.h"
#include "runtime/program/print_formatter.h"
#include "runtime/kernel/kernel.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"

namespace OCLRT {

PrintfHandler::PrintfHandler(Device &deviceArg) : device(deviceArg) {}

PrintfHandler::~PrintfHandler() {
    device.getMemoryManager()->freeGraphicsMemory(printfSurface);
}

PrintfHandler *PrintfHandler::create(const MultiDispatchInfo &multiDispatchInfo, Device &device) {
    if (multiDispatchInfo.usesStatelessPrintfSurface() ||
        (multiDispatchInfo.begin()->getKernel()->checkIfIsParentKernelAndBlocksUsesPrintf())) {
        return new PrintfHandler(device);
    }
    return nullptr;
}

void PrintfHandler::prepareDispatch(const MultiDispatchInfo &multiDispatchInfo) {
    auto printfSurfaceSize = device.getDeviceInfo().printfBufferSize;
    if (printfSurfaceSize == 0) {
        return;
    }
    kernel = multiDispatchInfo.begin()->getKernel();
    printfSurface = device.getMemoryManager()->allocateGraphicsMemoryInPreferredPool(true, true, false, false, nullptr, printfSurfaceSize, GraphicsAllocation::AllocationType::PRINTF_SURFACE);

    *reinterpret_cast<uint32_t *>(printfSurface->getUnderlyingBuffer()) = printfSurfaceInitialDataSize;

    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getCrossThreadData()),
                                        kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->DataParamOffset);

    patchWithRequiredSize(printfPatchAddress, kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->DataParamSize, (uintptr_t)printfSurface->getGpuAddressToPatch());
    if (kernel->requiresSshForBuffers()) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                      kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->SurfaceStateHeapOffset);
        void *addressToPatch = printfSurface->getUnderlyingBuffer();
        size_t sizeToPatch = printfSurface->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&device, surfaceState, sizeToPatch, addressToPatch, printfSurface);
    }
}

void PrintfHandler::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    commandStreamReceiver.makeResident(*printfSurface);
}

void PrintfHandler::printEnqueueOutput() {
    PrintFormatter printFormatter(*kernel, *printfSurface);
    printFormatter.printKernelOutput();
}
} // namespace OCLRT

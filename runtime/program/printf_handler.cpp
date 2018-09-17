/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
        (multiDispatchInfo.peekMainKernel()->checkIfIsParentKernelAndBlocksUsesPrintf())) {
        return new PrintfHandler(device);
    }
    return nullptr;
}

void PrintfHandler::prepareDispatch(const MultiDispatchInfo &multiDispatchInfo) {
    auto printfSurfaceSize = device.getDeviceInfo().printfBufferSize;
    if (printfSurfaceSize == 0) {
        return;
    }
    kernel = multiDispatchInfo.peekMainKernel();
    printfSurface = device.getMemoryManager()->allocateGraphicsMemoryInPreferredPool(AllocationFlags(true), 0, nullptr, printfSurfaceSize, GraphicsAllocation::AllocationType::PRINTF_SURFACE);

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

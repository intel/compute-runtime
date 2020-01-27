/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "printf_handler.h"

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "core/program/print_formatter.h"
#include "runtime/device/cl_device.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"

namespace NEO {

PrintfHandler::PrintfHandler(ClDevice &deviceArg) : device(deviceArg) {}

PrintfHandler::~PrintfHandler() {
    device.getMemoryManager()->freeGraphicsMemory(printfSurface);
}

PrintfHandler *PrintfHandler::create(const MultiDispatchInfo &multiDispatchInfo, ClDevice &device) {
    if (multiDispatchInfo.usesStatelessPrintfSurface()) {
        return new PrintfHandler(device);
    }
    auto mainKernel = multiDispatchInfo.peekMainKernel();
    if ((mainKernel != nullptr) &&
        mainKernel->checkIfIsParentKernelAndBlocksUsesPrintf()) {
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
    printfSurface = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(), printfSurfaceSize, GraphicsAllocation::AllocationType::PRINTF_SURFACE});
    *reinterpret_cast<uint32_t *>(printfSurface->getUnderlyingBuffer()) = printfSurfaceInitialDataSize;

    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getCrossThreadData()),
                                        kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->DataParamOffset);

    patchWithRequiredSize(printfPatchAddress, kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->DataParamSize, (uintptr_t)printfSurface->getGpuAddressToPatch());
    if (kernel->requiresSshForBuffers()) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                      kernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->SurfaceStateHeapOffset);
        void *addressToPatch = printfSurface->getUnderlyingBuffer();
        size_t sizeToPatch = printfSurface->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&device, surfaceState, sizeToPatch, addressToPatch, 0, printfSurface, 0, 0);
    }
}

void PrintfHandler::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    commandStreamReceiver.makeResident(*printfSurface);
}

void PrintfHandler::printEnqueueOutput() {
    PrintFormatter printFormatter(reinterpret_cast<const uint8_t *>(printfSurface->getUnderlyingBuffer()), static_cast<uint32_t>(printfSurface->getUnderlyingBufferSize()),
                                  kernel->is32Bit(), kernel->getKernelInfo().patchInfo.stringDataMap);
    printFormatter.printKernelOutput();
}
} // namespace NEO

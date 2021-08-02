/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "printf_handler.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/print_formatter.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

const uint32_t PrintfHandler::printfSurfaceInitialDataSize;

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
    auto printfSurfaceSize = device.getSharedDeviceInfo().printfBufferSize;
    if (printfSurfaceSize == 0) {
        return;
    }
    auto rootDeviceIndex = device.getRootDeviceIndex();
    kernel = multiDispatchInfo.peekMainKernel();
    printfSurface = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, printfSurfaceSize, GraphicsAllocation::AllocationType::PRINTF_SURFACE, device.getDeviceBitfield()});

    auto &hwInfo = device.getHardwareInfo();
    auto &helper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    MemoryTransferHelper::transferMemoryToAllocation(helper.isBlitCopyRequiredForLocalMemory(hwInfo, *printfSurface),
                                                     device.getDevice(), printfSurface, 0, &printfSurfaceInitialDataSize,
                                                     sizeof(printfSurfaceInitialDataSize));

    const auto &printfSurfaceArg = kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getCrossThreadData()), printfSurfaceArg.stateless);
    patchWithRequiredSize(printfPatchAddress, printfSurfaceArg.pointerSize, (uintptr_t)printfSurface->getGpuAddressToPatch());
    if (isValidOffset(printfSurfaceArg.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()), printfSurfaceArg.bindful);
        void *addressToPatch = printfSurface->getUnderlyingBuffer();
        size_t sizeToPatch = printfSurface->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&device.getDevice(), surfaceState, false, false, sizeToPatch, addressToPatch, 0, printfSurface, 0, 0,
                                kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics,
                                kernel->areMultipleSubDevicesInContext());
    }
}

void PrintfHandler::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    commandStreamReceiver.makeResident(*printfSurface);
}

void PrintfHandler::printEnqueueOutput() {
    auto &helper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
    if (helper.allowStatelessCompression(device.getHardwareInfo())) {
        auto &bcsEngine = device.getEngine(EngineHelpers::getBcsEngineType(device.getHardwareInfo(), device.getSelectorCopyEngine()), EngineUsage::Regular);
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(
            BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::AuxToNonAux,
                                                                 printfSurface, bcsEngine.commandStreamReceiver->getClearColorAllocation()));
        bcsEngine.commandStreamReceiver->blitBuffer(blitPropertiesContainer, true, false, device.getDevice());
    }

    PrintFormatter printFormatter(reinterpret_cast<const uint8_t *>(printfSurface->getUnderlyingBuffer()), static_cast<uint32_t>(printfSurface->getUnderlyingBufferSize()),
                                  kernel->is32Bit(),
                                  kernel->getDescriptor().kernelAttributes.flags.usesStringMapForPrintf ? &kernel->getDescriptor().kernelMetadata.printfStringsMap : nullptr);
    printFormatter.printKernelOutput();
}
} // namespace NEO

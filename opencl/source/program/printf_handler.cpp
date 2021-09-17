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
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/print_formatter.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

PrintfHandler::PrintfHandler(ClDevice &deviceArg) : device(deviceArg) {
    printfSurfaceInitialDataSizePtr = std::make_unique<uint32_t>();
    *printfSurfaceInitialDataSizePtr = sizeof(uint32_t);
}

PrintfHandler::~PrintfHandler() {
    device.getMemoryManager()->freeGraphicsMemory(printfSurface);
}

PrintfHandler *PrintfHandler::create(const MultiDispatchInfo &multiDispatchInfo, ClDevice &device) {
    if (multiDispatchInfo.usesStatelessPrintfSurface()) {
        return new PrintfHandler(device);
    }
    auto mainKernel = multiDispatchInfo.peekMainKernel();
    if (mainKernel != nullptr) {
        if (mainKernel->checkIfIsParentKernelAndBlocksUsesPrintf() || mainKernel->getImplicitArgs()) {
            return new PrintfHandler(device);
        }
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
                                                     device.getDevice(), printfSurface, 0, printfSurfaceInitialDataSizePtr.get(),
                                                     sizeof(*printfSurfaceInitialDataSizePtr.get()));

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
    auto pImplicitArgs = kernel->getImplicitArgs();
    if (pImplicitArgs) {
        pImplicitArgs->printfBufferPtr = printfSurface->getGpuAddress();
    }
}

void PrintfHandler::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    commandStreamReceiver.makeResident(*printfSurface);
}

void PrintfHandler::printEnqueueOutput() {
    const auto &hwInfoConfig = *HwInfoConfig::get(device.getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.allowStatelessCompression(device.getHardwareInfo())) {
        auto printOutputSize = static_cast<uint32_t>(printfSurface->getUnderlyingBufferSize());
        auto printOutputDecompressed = std::make_unique<uint8_t[]>(printOutputSize);
        auto &bcsEngine = device.getEngine(EngineHelpers::getBcsEngineType(device.getHardwareInfo(), device.getDeviceBitfield(), device.getSelectorCopyEngine(), true), EngineUsage::Regular);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(
            BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                            *bcsEngine.commandStreamReceiver, printfSurface, nullptr,
                                                            printOutputDecompressed.get(),
                                                            printfSurface->getGpuAddress(),
                                                            0, 0, 0, Vec3<size_t>(printOutputSize, 0, 0), 0, 0, 0, 0));
        bcsEngine.commandStreamReceiver->blitBuffer(blitPropertiesContainer, true, false, device.getDevice());

        PrintFormatter printFormatter(printOutputDecompressed.get(), printOutputSize,
                                      kernel->is32Bit(),
                                      kernel->getDescriptor().kernelAttributes.flags.usesStringMapForPrintf ? &kernel->getDescriptor().kernelMetadata.printfStringsMap : nullptr);
        printFormatter.printKernelOutput();
        return;
    }

    PrintFormatter printFormatter(reinterpret_cast<const uint8_t *>(printfSurface->getUnderlyingBuffer()), static_cast<uint32_t>(printfSurface->getUnderlyingBufferSize()),
                                  kernel->is32Bit(),
                                  kernel->getDescriptor().kernelAttributes.flags.usesStringMapForPrintf ? &kernel->getDescriptor().kernelMetadata.printfStringsMap : nullptr);
    printFormatter.printKernelOutput();
}
} // namespace NEO

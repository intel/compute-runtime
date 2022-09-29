/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "printf_handler.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/print_formatter.h"

#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

PrintfHandler::PrintfHandler(Device &deviceArg) : device(deviceArg) {
    printfSurfaceInitialDataSizePtr = std::make_unique<uint32_t>();
    *printfSurfaceInitialDataSizePtr = sizeof(uint32_t);
}

PrintfHandler::~PrintfHandler() {
    device.getMemoryManager()->freeGraphicsMemory(printfSurface);
}

PrintfHandler *PrintfHandler::create(const MultiDispatchInfo &multiDispatchInfo, Device &device) {
    if (multiDispatchInfo.usesStatelessPrintfSurface()) {
        return new PrintfHandler(device);
    }
    return nullptr;
}

void PrintfHandler::prepareDispatch(const MultiDispatchInfo &multiDispatchInfo) {
    auto printfSurfaceSize = device.getDeviceInfo().printfBufferSize;
    if (printfSurfaceSize == 0) {
        return;
    }
    auto rootDeviceIndex = device.getRootDeviceIndex();
    kernel = multiDispatchInfo.peekMainKernel();
    printfSurface = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, printfSurfaceSize, AllocationType::PRINTF_SURFACE, device.getDeviceBitfield()});

    auto &hwInfo = device.getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *printfSurface),
                                                     device, printfSurface, 0, printfSurfaceInitialDataSizePtr.get(),
                                                     sizeof(*printfSurfaceInitialDataSizePtr.get()));

    const auto &printfSurfaceArg = kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getCrossThreadData()), printfSurfaceArg.stateless);
    patchWithRequiredSize(printfPatchAddress, printfSurfaceArg.pointerSize, (uintptr_t)printfSurface->getGpuAddressToPatch());
    if (isValidOffset(printfSurfaceArg.bindful)) {
        auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()), printfSurfaceArg.bindful);
        void *addressToPatch = printfSurface->getUnderlyingBuffer();
        size_t sizeToPatch = printfSurface->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&device, surfaceState, false, false, sizeToPatch, addressToPatch, 0, printfSurface, 0, 0,
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

bool PrintfHandler::printEnqueueOutput() {
    auto &hwInfo = device.getHardwareInfo();

    auto usesStringMap = kernel->getDescriptor().kernelAttributes.usesStringMap();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto printfOutputBuffer = reinterpret_cast<const uint8_t *>(printfSurface->getUnderlyingBuffer());
    auto printfOutputSize = static_cast<uint32_t>(printfSurface->getUnderlyingBufferSize());
    std::unique_ptr<uint8_t[]> printfOutputDecompressed;

    if (hwInfoConfig.allowStatelessCompression(hwInfo) || hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *printfSurface)) {
        printfOutputDecompressed = std::make_unique<uint8_t[]>(printfOutputSize);
        printfOutputBuffer = printfOutputDecompressed.get();
        auto &bcsEngine = device.getEngine(EngineHelpers::getBcsEngineType(hwInfo, device.getDeviceBitfield(), device.getSelectorCopyEngine(), true), EngineUsage::Regular);

        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(
            BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                            *bcsEngine.commandStreamReceiver, printfSurface, nullptr,
                                                            printfOutputDecompressed.get(),
                                                            printfSurface->getGpuAddress(),
                                                            0, 0, 0, Vec3<size_t>(printfOutputSize, 0, 0), 0, 0, 0, 0));

        const auto newTaskCount = bcsEngine.commandStreamReceiver->flushBcsTask(blitPropertiesContainer, true, false, device);
        if (!newTaskCount) {
            return false;
        }
    }

    PrintFormatter printFormatter(printfOutputBuffer, printfOutputSize, kernel->is32Bit(),
                                  usesStringMap ? &kernel->getDescriptor().kernelMetadata.printfStringsMap : nullptr);
    printFormatter.printKernelOutput();

    return true;
}

} // namespace NEO

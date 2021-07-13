/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_info.h"
#include "shared/source/program/program_initialization.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/program.h"

#include "program_debug_data.h"

#include <algorithm>

using namespace iOpenCL;

namespace NEO {
extern bool familyEnabled[];

const KernelInfo *Program::getKernelInfo(
    const char *kernelName, uint32_t rootDeviceIndex) const {
    if (kernelName == nullptr) {
        return nullptr;
    }

    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;

    auto it = std::find_if(kernelInfoArray.begin(), kernelInfoArray.end(),
                           [=](const KernelInfo *kInfo) { return (0 == strcmp(kInfo->kernelDescriptor.kernelMetadata.kernelName.c_str(), kernelName)); });

    return (it != kernelInfoArray.end()) ? *it : nullptr;
}

size_t Program::getNumKernels() const {
    return buildInfos[clDevices[0]->getRootDeviceIndex()].kernelInfoArray.size();
}

const KernelInfo *Program::getKernelInfo(size_t ordinal, uint32_t rootDeviceIndex) const {
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    DEBUG_BREAK_IF(ordinal >= kernelInfoArray.size());
    return kernelInfoArray[ordinal];
}

cl_int Program::linkBinary(Device *pDevice, const void *constantsInitData, const void *variablesInitData) {
    auto linkerInput = getLinkerInput(pDevice->getRootDeviceIndex());
    if (linkerInput == nullptr) {
        return CL_SUCCESS;
    }
    auto rootDeviceIndex = pDevice->getRootDeviceIndex();
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    Linker linker(*linkerInput);
    Linker::SegmentInfo globals;
    Linker::SegmentInfo constants;
    Linker::SegmentInfo exportedFunctions;
    GraphicsAllocation *globalsForPatching = getGlobalSurface(rootDeviceIndex);
    GraphicsAllocation *constantsForPatching = getConstantSurface(rootDeviceIndex);
    if (globalsForPatching != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(globalsForPatching->getGpuAddress());
        globals.segmentSize = globalsForPatching->getUnderlyingBufferSize();
    }
    if (constantsForPatching != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(constantsForPatching->getGpuAddress());
        constants.segmentSize = constantsForPatching->getUnderlyingBufferSize();
    }
    if (linkerInput->getExportedFunctionsSegmentId() >= 0) {
        // Exported functions reside in instruction heap of one of kernels
        auto exportedFunctionHeapId = linkerInput->getExportedFunctionsSegmentId();
        buildInfos[rootDeviceIndex].exportedFunctionsSurface = kernelInfoArray[exportedFunctionHeapId]->getGraphicsAllocation();
        exportedFunctions.gpuAddress = static_cast<uintptr_t>(buildInfos[rootDeviceIndex].exportedFunctionsSurface->getGpuAddressToPatch());
        exportedFunctions.segmentSize = buildInfos[rootDeviceIndex].exportedFunctionsSurface->getUnderlyingBufferSize();
    }
    Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
    if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        patchedIsaTempStorage.reserve(kernelInfoArray.size());
        for (const auto &kernelInfo : kernelInfoArray) {
            auto &kernHeapInfo = kernelInfo->heapInfo;
            const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.KernelHeapSize));
            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), kernHeapInfo.KernelHeapSize});
        }
    }

    Linker::UnresolvedExternals unresolvedExternalsInfo;
    bool linkSuccess = LinkingStatus::LinkedFully == linker.link(globals, constants, exportedFunctions,
                                                                 globalsForPatching, constantsForPatching,
                                                                 isaSegmentsForPatching, unresolvedExternalsInfo,
                                                                 pDevice, constantsInitData, variablesInitData);
    setSymbols(rootDeviceIndex, linker.extractRelocatedSymbols());
    if (false == linkSuccess) {
        std::vector<std::string> kernelNames;
        for (const auto &kernelInfo : kernelInfoArray) {
            kernelNames.push_back("kernel : " + kernelInfo->kernelDescriptor.kernelMetadata.kernelName);
        }
        auto error = constructLinkerErrorMessage(unresolvedExternalsInfo, kernelNames);
        updateBuildLog(pDevice->getRootDeviceIndex(), error.c_str(), error.size());
        return CL_INVALID_BINARY;
    } else if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        for (const auto &kernelInfo : kernelInfoArray) {
            if (nullptr == kernelInfo->getGraphicsAllocation()) {
                continue;
            }
            auto &kernHeapInfo = kernelInfo->heapInfo;
            auto segmentId = &kernelInfo - &kernelInfoArray[0];
            auto &hwInfo = pDevice->getHardwareInfo();
            auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
            MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelInfo->getGraphicsAllocation()),
                                                             *pDevice, kernelInfo->getGraphicsAllocation(), 0, isaSegmentsForPatching[segmentId].hostPointer,
                                                             static_cast<size_t>(kernHeapInfo.KernelHeapSize));
        }
    }
    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->getSymbols(pDevice->getRootDeviceIndex())));
    return CL_SUCCESS;
}

cl_int Program::processGenBinary(const ClDevice &clDevice) {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    if (nullptr == this->buildInfos[rootDeviceIndex].unpackedDeviceBinary) {
        return CL_INVALID_BINARY;
    }

    cleanCurrentKernelInfo(rootDeviceIndex);
    auto &buildInfo = buildInfos[rootDeviceIndex];

    if (buildInfo.constantSurface || buildInfo.globalSurface) {
        clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.constantSurface);
        clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.globalSurface);
        buildInfo.constantSurface = nullptr;
        buildInfo.globalSurface = nullptr;
    }

    ProgramInfo programInfo;
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    binary.targetDevice.grfSize = clDevice.getDevice().getHardwareInfo().capabilityTable.grfSize;
    std::string decodeErrors;
    std::string decodeWarnings;

    DecodeError decodeError;
    DeviceBinaryFormat singleDeviceBinaryFormat;
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings);
    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (DecodeError::Success != decodeError) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return CL_INVALID_BINARY;
    }

    return this->processProgramInfo(programInfo, clDevice);
}

cl_int Program::processProgramInfo(ProgramInfo &src, const ClDevice &clDevice) {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    size_t slmNeeded = getMaxInlineSlmNeeded(src);
    size_t slmAvailable = 0U;
    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    LinkerInput *linkerInput = nullptr;
    slmAvailable = static_cast<size_t>(clDevice.getSharedDeviceInfo().localMemSize);
    deviceInfoConstants.maxWorkGroupSize = static_cast<uint32_t>(clDevice.getSharedDeviceInfo().maxWorkGroupSize);
    deviceInfoConstants.computeUnitsUsedForScratch = clDevice.getSharedDeviceInfo().computeUnitsUsedForScratch;
    deviceInfoConstants.slmWindowSize = static_cast<uint32_t>(clDevice.getSharedDeviceInfo().localMemSize);
    if (requiresLocalMemoryWindowVA(src)) {
        deviceInfoConstants.slmWindow = this->executionEnvironment.memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }
    linkerInput = src.linkerInput.get();
    setLinkerInput(rootDeviceIndex, std::move(src.linkerInput));

    if (slmNeeded > slmAvailable) {
        return CL_OUT_OF_RESOURCES;
    }

    kernelInfoArray = std::move(src.kernelInfos);
    auto svmAllocsManager = context ? context->getSVMAllocsManager() : nullptr;
    if (src.globalConstants.size != 0) {
        buildInfos[rootDeviceIndex].constantSurface = allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), src.globalConstants.size, true, linkerInput, src.globalConstants.initData);
    }

    buildInfos[rootDeviceIndex].globalVarTotalSize = src.globalVariables.size;

    if (src.globalVariables.size != 0) {
        buildInfos[rootDeviceIndex].globalSurface = allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), src.globalVariables.size, false, linkerInput, src.globalVariables.initData);
        if (clDevice.areOcl21FeaturesEnabled() == false) {
            buildInfos[rootDeviceIndex].globalVarTotalSize = 0u;
        }
    }

    for (auto &kernelInfo : kernelInfoArray) {
        cl_int retVal = CL_SUCCESS;
        if (kernelInfo->heapInfo.KernelHeapSize) {
            retVal = kernelInfo->createKernelAllocation(clDevice.getDevice(), isBuiltIn) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
        }

        if (retVal != CL_SUCCESS) {
            return retVal;
        }

        if (kernelInfo->hasDeviceEnqueue()) {
            buildInfos[rootDeviceIndex].parentKernelInfoArray.push_back(kernelInfo);
        }
        if (kernelInfo->requiresSubgroupIndependentForwardProgress()) {
            buildInfos[rootDeviceIndex].subgroupKernelInfoArray.push_back(kernelInfo);
        }

        kernelInfo->apply(deviceInfoConstants);
    }

    return linkBinary(&clDevice.getDevice(), src.globalConstants.initData, src.globalVariables.initData);
}

void Program::processDebugData(uint32_t rootDeviceIndex) {
    if (debugData != nullptr) {
        auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
        SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<SProgramDebugDataHeaderIGC *>(debugData.get());

        DEBUG_BREAK_IF(programDebugHeader->NumberOfKernels != kernelInfoArray.size());

        const SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(SProgramDebugDataHeaderIGC)));
        const char *kernelName = nullptr;
        const char *kernelDebugData = nullptr;

        for (uint32_t i = 0; i < programDebugHeader->NumberOfKernels; i++) {
            kernelName = reinterpret_cast<const char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));

            auto kernelInfo = kernelInfoArray[i];
            UNRECOVERABLE_IF(kernelInfo->kernelDescriptor.kernelMetadata.kernelName.compare(0, kernelInfo->kernelDescriptor.kernelMetadata.kernelName.size(), kernelName) != 0);

            kernelDebugData = ptrOffset(kernelName, kernelDebugHeader->KernelNameSize);

            kernelInfo->debugData.vIsa = kernelDebugData;
            kernelInfo->debugData.genIsa = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);
            kernelInfo->debugData.vIsaSize = kernelDebugHeader->SizeVisaDbgInBytes;
            kernelInfo->debugData.genIsaSize = kernelDebugHeader->SizeGenIsaDbgInBytes;

            kernelDebugData = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes + kernelDebugHeader->SizeGenIsaDbgInBytes);
            kernelDebugHeader = reinterpret_cast<const SKernelDebugDataHeaderIGC *>(kernelDebugData);
        }
    }
}

} // namespace NEO

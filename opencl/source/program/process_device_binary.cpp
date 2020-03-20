/*
 * Copyright (C) 2017-2020 Intel Corporation
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
    const char *kernelName) const {
    if (kernelName == nullptr) {
        return nullptr;
    }

    auto it = std::find_if(kernelInfoArray.begin(), kernelInfoArray.end(),
                           [=](const KernelInfo *kInfo) { return (0 == strcmp(kInfo->name.c_str(), kernelName)); });

    return (it != kernelInfoArray.end()) ? *it : nullptr;
}

size_t Program::getNumKernels() const {
    return kernelInfoArray.size();
}

const KernelInfo *Program::getKernelInfo(size_t ordinal) const {
    DEBUG_BREAK_IF(ordinal >= kernelInfoArray.size());
    return kernelInfoArray[ordinal];
}

cl_int Program::linkBinary() {
    if (linkerInput == nullptr) {
        return CL_SUCCESS;
    }
    Linker linker(*linkerInput);
    Linker::SegmentInfo globals;
    Linker::SegmentInfo constants;
    Linker::SegmentInfo exportedFunctions;
    Linker::PatchableSegment globalsForPatching;
    Linker::PatchableSegment constantsForPatching;
    if (this->globalSurface != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(this->globalSurface->getGpuAddress());
        globals.segmentSize = this->globalSurface->getUnderlyingBufferSize();
        globalsForPatching.hostPointer = this->globalSurface->getUnderlyingBuffer();
        globalsForPatching.segmentSize = this->globalSurface->getUnderlyingBufferSize();
    }
    if (this->constantSurface != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(this->constantSurface->getGpuAddress());
        constants.segmentSize = this->constantSurface->getUnderlyingBufferSize();
        constantsForPatching.hostPointer = this->constantSurface->getUnderlyingBuffer();
        constantsForPatching.segmentSize = this->constantSurface->getUnderlyingBufferSize();
    }
    if (this->linkerInput->getExportedFunctionsSegmentId() >= 0) {
        // Exported functions reside in instruction heap of one of kernels
        auto exportedFunctionHeapId = this->linkerInput->getExportedFunctionsSegmentId();
        this->exportedFunctionsSurface = this->kernelInfoArray[exportedFunctionHeapId]->getGraphicsAllocation();
        exportedFunctions.gpuAddress = static_cast<uintptr_t>(exportedFunctionsSurface->getGpuAddressToPatch());
        exportedFunctions.segmentSize = exportedFunctionsSurface->getUnderlyingBufferSize();
    }
    Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
    if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        patchedIsaTempStorage.reserve(this->kernelInfoArray.size());
        for (const auto &kernelInfo : this->kernelInfoArray) {
            auto &kernHeapInfo = kernelInfo->heapInfo;
            const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.pKernelHeader->KernelHeapSize));
            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), kernHeapInfo.pKernelHeader->KernelHeapSize});
        }
    }

    Linker::UnresolvedExternals unresolvedExternalsInfo;
    bool linkSuccess = linker.link(globals, constants, exportedFunctions,
                                   globalsForPatching, constantsForPatching,
                                   isaSegmentsForPatching, unresolvedExternalsInfo);
    this->symbols = linker.extractRelocatedSymbols();
    if (false == linkSuccess) {
        std::vector<std::string> kernelNames;
        for (const auto &kernelInfo : this->kernelInfoArray) {
            kernelNames.push_back("kernel : " + kernelInfo->name);
        }
        auto error = constructLinkerErrorMessage(unresolvedExternalsInfo, kernelNames);
        updateBuildLog(pDevice, error.c_str(), error.size());
        return CL_INVALID_BINARY;
    } else if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        for (const auto &kernelInfo : this->kernelInfoArray) {
            if (nullptr == kernelInfo->getGraphicsAllocation()) {
                continue;
            }
            auto &kernHeapInfo = kernelInfo->heapInfo;
            auto segmentId = &kernelInfo - &this->kernelInfoArray[0];
            this->pDevice->getMemoryManager()->copyMemoryToAllocation(kernelInfo->getGraphicsAllocation(),
                                                                      isaSegmentsForPatching[segmentId].hostPointer,
                                                                      kernHeapInfo.pKernelHeader->KernelHeapSize);
        }
    }
    return CL_SUCCESS;
}

cl_int Program::processGenBinary() {
    if (nullptr == this->unpackedDeviceBinary) {
        return CL_INVALID_BINARY;
    }

    cleanCurrentKernelInfo();
    if (this->constantSurface || this->globalSurface) {
        pDevice->getMemoryManager()->freeGraphicsMemory(this->constantSurface);
        pDevice->getMemoryManager()->freeGraphicsMemory(this->globalSurface);
        this->constantSurface = nullptr;
        this->globalSurface = nullptr;
    }

    ProgramInfo programInfo;
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
    SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    std::string decodeErrors;
    std::string decodeWarnings;

    DecodeError decodeError;
    DeviceBinaryFormat singleDeviceBinaryFormat;
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings);
    if (decodeWarnings.empty() == false) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (DecodeError::Success != decodeError) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return CL_INVALID_BINARY;
    }

    return this->processProgramInfo(programInfo);
}

cl_int Program::processProgramInfo(ProgramInfo &src) {
    size_t slmNeeded = getMaxInlineSlmNeeded(src);
    size_t slmAvailable = 0U;
    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    if (this->pDevice) {
        slmAvailable = static_cast<size_t>(this->pDevice->getDeviceInfo().localMemSize);
        deviceInfoConstants.maxWorkGroupSize = (uint32_t)this->pDevice->getDeviceInfo().maxWorkGroupSize;
        deviceInfoConstants.computeUnitsUsedForScratch = this->pDevice->getDeviceInfo().computeUnitsUsedForScratch;
        deviceInfoConstants.slmWindowSize = (uint32_t)this->pDevice->getDeviceInfo().localMemSize;
        if (requiresLocalMemoryWindowVA(src)) {
            deviceInfoConstants.slmWindow = this->executionEnvironment.memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
        }
    }
    if (slmNeeded > slmAvailable) {
        return CL_OUT_OF_RESOURCES;
    }

    this->linkerInput = std::move(src.linkerInput);
    this->kernelInfoArray = std::move(src.kernelInfos);
    auto svmAllocsManager = context ? context->getSVMAllocsManager() : nullptr;
    if (src.globalConstants.size != 0) {
        UNRECOVERABLE_IF(nullptr == pDevice);
        this->constantSurface = allocateGlobalsSurface(svmAllocsManager, *pDevice, src.globalConstants.size, true, linkerInput.get(), src.globalConstants.initData);
    }

    if (src.globalVariables.size != 0) {
        UNRECOVERABLE_IF(nullptr == pDevice);
        this->globalSurface = allocateGlobalsSurface(svmAllocsManager, *pDevice, src.globalVariables.size, false, linkerInput.get(), src.globalVariables.initData);
    }

    this->globalVarTotalSize = src.globalVariables.size;

    for (auto &kernelInfo : this->kernelInfoArray) {
        cl_int retVal = CL_SUCCESS;
        if (kernelInfo->heapInfo.pKernelHeader->KernelHeapSize && this->pDevice) {
            retVal = kernelInfo->createKernelAllocation(this->pDevice->getRootDeviceIndex(), this->pDevice->getMemoryManager()) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
        }

        DEBUG_BREAK_IF(kernelInfo->heapInfo.pKernelHeader->KernelHeapSize && !this->pDevice);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }

        if (kernelInfo->hasDeviceEnqueue()) {
            parentKernelInfoArray.push_back(kernelInfo);
        }
        if (kernelInfo->requiresSubgroupIndependentForwardProgress()) {
            subgroupKernelInfoArray.push_back(kernelInfo);
        }

        kernelInfo->apply(deviceInfoConstants);
    }

    return linkBinary();
}

void Program::processDebugData() {
    if (debugData != nullptr) {
        SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<SProgramDebugDataHeaderIGC *>(debugData.get());

        DEBUG_BREAK_IF(programDebugHeader->NumberOfKernels != kernelInfoArray.size());

        const SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(SProgramDebugDataHeaderIGC)));
        const char *kernelName = nullptr;
        const char *kernelDebugData = nullptr;

        for (uint32_t i = 0; i < programDebugHeader->NumberOfKernels; i++) {
            kernelName = reinterpret_cast<const char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));

            auto kernelInfo = kernelInfoArray[i];
            UNRECOVERABLE_IF(kernelInfo->name.compare(0, kernelInfo->name.size(), kernelName) != 0);

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

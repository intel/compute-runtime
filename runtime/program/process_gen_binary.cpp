/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/device_binary_format/patchtokens_dumper.h"
#include "core/device_binary_format/patchtokens_validator.inl"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/string.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "core/program/program_info.h"
#include "core/program/program_info_from_patchtokens.h"
#include "core/program/program_initialization.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/program/kernel_info.h"
#include "runtime/program/kernel_info_from_patchtokens.h"
#include "runtime/program/program.h"

#include "patch_list.h"
#include "patch_shared.h"
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

cl_int Program::isHandled(const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram) const {
    std::string validatorErrMessage;
    std::string validatorWarnings;
    auto availableSlm = this->pDevice ? static_cast<size_t>(this->pDevice->getDeviceInfo().localMemSize) : 0U;
    auto validatorErr = PatchTokenBinary::validate(decodedProgram, availableSlm, *this, validatorErrMessage, validatorWarnings);
    if (validatorWarnings.empty() == false) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", validatorWarnings.c_str());
    }
    if (validatorErr != PatchTokenBinary::ValidatorError::Success) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", validatorErrMessage.c_str());
        switch (validatorErr) {
        default:
            return CL_INVALID_BINARY;
        case PatchTokenBinary::ValidatorError::NotEnoughSlm:
            return CL_OUT_OF_RESOURCES;
        }
    }
    return CL_SUCCESS;
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
    cleanCurrentKernelInfo();
    if (this->constantSurface || this->globalSurface) {
        pDevice->getMemoryManager()->freeGraphicsMemory(this->constantSurface);
        pDevice->getMemoryManager()->freeGraphicsMemory(this->globalSurface);
        this->constantSurface = nullptr;
        this->globalSurface = nullptr;
    }

    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(genBinary.get()), genBinarySize);
    ProgramInfo programInfo;
    auto ret = this->processPatchTokensBinary(blob, programInfo);
    if (CL_SUCCESS != ret) {
        return ret;
    }

    return this->processProgramInfo(programInfo);
}

cl_int Program::processPatchTokensBinary(ArrayRef<const uint8_t> src, ProgramInfo &dst) {
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram = {};
    NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(src, decodedProgram);
    DBG_LOG(LogPatchTokens, NEO::PatchTokenBinary::asString(decodedProgram).c_str());
    cl_int retVal = this->isHandled(decodedProgram);
    if (CL_SUCCESS != retVal) {
        return retVal;
    }

    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    if (this->pDevice) {
        deviceInfoConstants.maxWorkGroupSize = (uint32_t)this->pDevice->getDeviceInfo().maxWorkGroupSize;
        deviceInfoConstants.computeUnitsUsedForScratch = this->pDevice->getDeviceInfo().computeUnitsUsedForScratch;
        deviceInfoConstants.slmWindowSize = (uint32_t)this->pDevice->getDeviceInfo().localMemSize;
        if (requiresLocalMemoryWindowVA(decodedProgram)) {
            deviceInfoConstants.slmWindow = this->executionEnvironment.memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
        }
    }

    NEO::populateProgramInfo(dst, decodedProgram, deviceInfoConstants);

    return CL_SUCCESS;
}

cl_int Program::processProgramInfo(ProgramInfo &src) {
    this->linkerInput = std::move(src.linkerInput);
    this->kernelInfoArray = std::move(src.kernelInfos);
    auto svmAllocsManager = context ? context->getSVMAllocsManager() : nullptr;
    if (src.globalConstants.size != 0) {
        UNRECOVERABLE_IF(nullptr == pDevice);
        this->constantSurface = allocateGlobalsSurface(svmAllocsManager, pDevice->getDevice(), src.globalConstants.size, true, linkerInput.get(), src.globalConstants.initData);
    }

    if (src.globalVariables.size != 0) {
        UNRECOVERABLE_IF(nullptr == pDevice);
        this->globalSurface = allocateGlobalsSurface(svmAllocsManager, pDevice->getDevice(), src.globalVariables.size, false, linkerInput.get(), src.globalVariables.initData);
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
    }

    return linkBinary();
}

bool Program::validateGenBinaryDevice(GFXCORE_FAMILY device) const {
    bool isValid = familyEnabled[device];

    return isValid;
}

bool Program::validateGenBinaryHeader(const iOpenCL::SProgramBinaryHeader *pGenBinaryHeader) const {
    return pGenBinaryHeader->Magic == MAGIC_CL &&
           pGenBinaryHeader->Version == CURRENT_ICBE_VERSION &&
           validateGenBinaryDevice(static_cast<GFXCORE_FAMILY>(pGenBinaryHeader->Device));
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

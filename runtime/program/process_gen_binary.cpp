/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/string.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "runtime/compiler_interface/patchtokens_dumper.h"
#include "runtime/compiler_interface/patchtokens_validator.inl"
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

std::string Program::getKernelNamesString() const {
    std::string semiColonDelimitedKernelNameStr;

    for (auto kernelInfo : kernelInfoArray) {
        if (!semiColonDelimitedKernelNameStr.empty()) {
            semiColonDelimitedKernelNameStr += ';';
        }
        semiColonDelimitedKernelNameStr += kernelInfo->name;
    }

    return semiColonDelimitedKernelNameStr;
}

void Program::populateKernelInfo(
    const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram,
    uint32_t kernelNum,
    cl_int &retVal) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    const PatchTokenBinary::KernelFromPatchtokens &decodedKernel = decodedProgram.kernels[kernelNum];

    NEO::populateKernelInfo(*kernelInfo, decodedKernel);
    retVal = kernelInfo->resolveKernelInfo();
    if (retVal != CL_SUCCESS) {
        return;
    }

    kernelInfo->gpuPointerSize = decodedProgram.header->GPUPointerSizeInBytes;

    if (decodedKernel.tokens.programSymbolTable) {
        prepareLinkerInputStorage();
        linkerInput->decodeExportedFunctionsSymbolTable(decodedKernel.tokens.programSymbolTable + 1, decodedKernel.tokens.programSymbolTable->NumEntries, kernelNum);
    }

    if (decodedKernel.tokens.programRelocationTable) {
        prepareLinkerInputStorage();
        linkerInput->decodeRelocationTable(decodedKernel.tokens.programRelocationTable + 1, decodedKernel.tokens.programRelocationTable->NumEntries, kernelNum);
    }

    if (kernelInfo->patchInfo.dataParameterStream && kernelInfo->patchInfo.dataParameterStream->DataParameterStreamSize) {
        uint32_t crossThreadDataSize = kernelInfo->patchInfo.dataParameterStream->DataParameterStreamSize;
        kernelInfo->crossThreadData = new char[crossThreadDataSize];
        memset(kernelInfo->crossThreadData, 0x00, crossThreadDataSize);

        uint32_t privateMemoryStatelessSizeOffset = kernelInfo->workloadInfo.privateMemoryStatelessSizeOffset;
        uint32_t localMemoryStatelessWindowSizeOffset = kernelInfo->workloadInfo.localMemoryStatelessWindowSizeOffset;
        uint32_t localMemoryStatelessWindowStartAddressOffset = kernelInfo->workloadInfo.localMemoryStatelessWindowStartAddressOffset;

        if (localMemoryStatelessWindowStartAddressOffset != 0xFFffFFff) {
            *(uintptr_t *)&(kernelInfo->crossThreadData[localMemoryStatelessWindowStartAddressOffset]) = reinterpret_cast<uintptr_t>(this->executionEnvironment.memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment));
        }

        if (localMemoryStatelessWindowSizeOffset != 0xFFffFFff) {
            *(uint32_t *)&(kernelInfo->crossThreadData[localMemoryStatelessWindowSizeOffset]) = (uint32_t)this->pDevice->getDeviceInfo().localMemSize;
        }

        if (kernelInfo->patchInfo.pAllocateStatelessPrivateSurface && (privateMemoryStatelessSizeOffset != 0xFFffFFff)) {
            *(uint32_t *)&(kernelInfo->crossThreadData[privateMemoryStatelessSizeOffset]) = kernelInfo->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize * this->getDevice(0).getDeviceInfo().computeUnitsUsedForScratch * kernelInfo->getMaxSimdSize();
        }

        if (kernelInfo->workloadInfo.maxWorkGroupSizeOffset != WorkloadInfo::undefinedOffset) {
            *(uint32_t *)&(kernelInfo->crossThreadData[kernelInfo->workloadInfo.maxWorkGroupSizeOffset]) = (uint32_t)this->getDevice(0).getDeviceInfo().maxWorkGroupSize;
        }
    }

    if (kernelInfo->heapInfo.pKernelHeader->KernelHeapSize && this->pDevice) {
        retVal = kernelInfo->createKernelAllocation(this->pDevice->getRootDeviceIndex(), this->pDevice->getMemoryManager()) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
    }

    DEBUG_BREAK_IF(kernelInfo->heapInfo.pKernelHeader->KernelHeapSize && !this->pDevice);
    if (retVal != CL_SUCCESS) {
        return;
    }

    if (kernelInfo->hasDeviceEnqueue()) {
        parentKernelInfoArray.push_back(kernelInfo.get());
    }
    if (kernelInfo->requiresSubgroupIndependentForwardProgress()) {
        subgroupKernelInfoArray.push_back(kernelInfo.get());
    }
    kernelInfoArray.push_back(kernelInfo.release());
}

inline uint64_t readMisalignedUint64(const uint64_t *address) {
    const uint32_t *addressBits = reinterpret_cast<const uint32_t *>(address);
    return static_cast<uint64_t>(static_cast<uint64_t>(addressBits[1]) << 32) | addressBits[0];
}

GraphicsAllocation *allocateGlobalsSurface(NEO::Context *ctx, NEO::Device *device, size_t size, bool constant, bool globalsAreExported, const void *initData) {
    UNRECOVERABLE_IF(device == nullptr);
    if (globalsAreExported && (ctx != nullptr) && (ctx->getSVMAllocsManager() != nullptr)) {
        NEO::SVMAllocsManager::SvmAllocationProperties svmProps = {};
        svmProps.coherent = false;
        svmProps.readOnly = constant;
        svmProps.hostPtrReadOnly = constant;
        auto ptr = ctx->getSVMAllocsManager()->createSVMAlloc(device->getRootDeviceIndex(), size, svmProps);
        DEBUG_BREAK_IF(ptr == nullptr);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto svmAlloc = ctx->getSVMAllocsManager()->getSVMAlloc(ptr);
        UNRECOVERABLE_IF(svmAlloc == nullptr);
        auto gpuAlloc = svmAlloc->gpuAllocation;
        UNRECOVERABLE_IF(gpuAlloc == nullptr);
        device->getMemoryManager()->copyMemoryToAllocation(gpuAlloc, initData, static_cast<uint32_t>(size));
        return ctx->getSVMAllocsManager()->getSVMAlloc(ptr)->gpuAllocation;
    } else {
        auto allocationType = constant ? GraphicsAllocation::AllocationType::CONSTANT_SURFACE : GraphicsAllocation::AllocationType::GLOBAL_SURFACE;
        auto gpuAlloc = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), size, allocationType});
        DEBUG_BREAK_IF(gpuAlloc == nullptr);
        if (gpuAlloc == nullptr) {
            return nullptr;
        }
        memcpy_s(gpuAlloc->getUnderlyingBuffer(), gpuAlloc->getUnderlyingBufferSize(), initData, size);
        return gpuAlloc;
    }
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

void Program::processProgramScopeMetadata(const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram) {
    if (decodedProgram.programScopeTokens.symbolTable != nullptr) {
        const auto patch = decodedProgram.programScopeTokens.symbolTable;
        this->prepareLinkerInputStorage();
        this->linkerInput->decodeGlobalVariablesSymbolTable(patch + 1, patch->NumEntries);
    }

    if (decodedProgram.programScopeTokens.allocateConstantMemorySurface.size() != 0) {
        pDevice->getMemoryManager()->freeGraphicsMemory(this->constantSurface);

        auto exportsGlobals = (linkerInput && linkerInput->getTraits().exportsGlobalConstants);
        size_t globalConstantsSurfaceSize = decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]->InlineDataSize;
        const void *globalConstantsInitData = NEO::PatchTokenBinary::getInlineData(decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]);
        this->constantSurface = allocateGlobalsSurface(context, pDevice, globalConstantsSurfaceSize, true, exportsGlobals, globalConstantsInitData);
    }

    if (decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size() != 0) {
        pDevice->getMemoryManager()->freeGraphicsMemory(this->globalSurface);

        auto exportsGlobals = (linkerInput && linkerInput->getTraits().exportsGlobalVariables);
        size_t globalVariablesSurfaceSize = decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]->InlineDataSize;
        const void *globalVariablesInitData = NEO::PatchTokenBinary::getInlineData(decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]);
        this->globalVarTotalSize = globalVariablesSurfaceSize;
        this->globalSurface = allocateGlobalsSurface(context, pDevice, globalVariablesSurfaceSize, false, exportsGlobals, globalVariablesInitData);
    }

    for (const auto &globalConstantPointerToken : decodedProgram.programScopeTokens.constantPointer) {
        NEO::GraphicsAllocation *srcSurface = this->constantSurface;
        if (globalConstantPointerToken->BufferType != PROGRAM_SCOPE_CONSTANT_BUFFER) {
            UNRECOVERABLE_IF(globalConstantPointerToken->BufferType != PROGRAM_SCOPE_GLOBAL_BUFFER);
            srcSurface = this->globalSurface;
        }
        UNRECOVERABLE_IF(srcSurface == nullptr);
        UNRECOVERABLE_IF(this->constantSurface == nullptr);
        auto offset = readMisalignedUint64(&globalConstantPointerToken->ConstantPointerOffset);
        UNRECOVERABLE_IF(this->constantSurface->getUnderlyingBufferSize() < ((offset + constantSurface->is32BitAllocation()) ? 4 : sizeof(uintptr_t)));
        void *patchOffset = ptrOffset(this->constantSurface->getUnderlyingBuffer(), static_cast<size_t>(offset));
        patchIncrement(patchOffset, constantSurface->is32BitAllocation() ? 4 : sizeof(uintptr_t), srcSurface->getGpuAddressToPatch());
    }

    for (const auto &globalVariablePointerToken : decodedProgram.programScopeTokens.globalPointer) {
        NEO::GraphicsAllocation *srcSurface = this->globalSurface;
        if (globalVariablePointerToken->BufferType != PROGRAM_SCOPE_GLOBAL_BUFFER) {
            UNRECOVERABLE_IF(globalVariablePointerToken->BufferType != PROGRAM_SCOPE_CONSTANT_BUFFER);
            srcSurface = this->constantSurface;
        }
        UNRECOVERABLE_IF(srcSurface == nullptr);
        UNRECOVERABLE_IF(this->globalSurface == nullptr);
        auto offset = readMisalignedUint64(&globalVariablePointerToken->GlobalPointerOffset);
        UNRECOVERABLE_IF(this->globalSurface->getUnderlyingBufferSize() < ((offset + globalSurface->is32BitAllocation()) ? 4 : sizeof(uintptr_t)));
        void *patchOffset = ptrOffset(this->globalSurface->getUnderlyingBuffer(), static_cast<size_t>(offset));
        patchIncrement(patchOffset, globalSurface->is32BitAllocation() ? 4 : sizeof(uintptr_t), srcSurface->getGpuAddressToPatch());
    }
}

cl_int Program::linkBinary() {
    if (linkerInput == nullptr) {
        return CL_SUCCESS;
    }
    Linker linker(*linkerInput);
    Linker::Segment globals;
    Linker::Segment constants;
    Linker::Segment exportedFunctions;
    if (this->globalSurface != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(this->globalSurface->getGpuAddress());
        globals.segmentSize = this->globalSurface->getUnderlyingBufferSize();
    }
    if (this->constantSurface != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(this->constantSurface->getGpuAddress());
        constants.segmentSize = this->constantSurface->getUnderlyingBufferSize();
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

    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(genBinary.get()), genBinarySize);
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram = {};
    NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(blob, decodedProgram);
    DBG_LOG(LogPatchTokens, NEO::PatchTokenBinary::asString(decodedProgram).c_str());
    cl_int retVal = this->isHandled(decodedProgram);
    if (CL_SUCCESS != retVal) {
        return retVal;
    }

    auto numKernels = decodedProgram.header->NumberOfKernels;
    for (uint32_t i = 0; i < numKernels && retVal == CL_SUCCESS; i++) {
        populateKernelInfo(decodedProgram, i, retVal);
    }

    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    processProgramScopeMetadata(decodedProgram);

    retVal = linkBinary();

    return retVal;
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

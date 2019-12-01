/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

#include "core/compiler_interface/compiler_interface.h"
#include "core/elf/writer.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/string.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/program/block_kernel_manager.h"
#include "runtime/program/kernel_info.h"

#include "compiler_options.h"

#include <sstream>

namespace NEO {

const std::string Program::clOptNameClVer("-cl-std=CL");

Program::Program(ExecutionEnvironment &executionEnvironment) : Program(executionEnvironment, nullptr, false) {
    numDevices = 0;
}

Program::Program(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn) : executionEnvironment(executionEnvironment),
                                                                                                 context(context),
                                                                                                 isBuiltIn(isBuiltIn) {
    if (this->context && !this->isBuiltIn) {
        this->context->incRefInternal();
    }
    blockKernelManager = new BlockKernelManager();
    pDevice = context ? context->getDevice(0) : nullptr;
    numDevices = 1;
    elfBinarySize = 0;
    genBinary = nullptr;
    genBinarySize = 0;
    irBinary = nullptr;
    irBinarySize = 0;
    debugData = nullptr;
    debugDataSize = 0;
    buildStatus = CL_BUILD_NONE;
    programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    isCreatedFromBinary = false;
    isProgramBinaryResolved = false;
    constantSurface = nullptr;
    globalSurface = nullptr;
    globalVarTotalSize = 0;
    programOptionVersion = 12u;
    allowNonUniform = false;
    char paramValue[32] = {};
    bool force32BitAddressess = false;

    if (pDevice) {
        pDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, nullptr);
        if (strstr(paramValue, "2.1")) {
            internalOptions = "-ocl-version=210 ";
        } else if (strstr(paramValue, "2.0")) {
            internalOptions = "-ocl-version=200 ";
        } else if (strstr(paramValue, "1.2")) {
            internalOptions = "-ocl-version=120 ";
        }
        force32BitAddressess = pDevice->getDeviceInfo().force32BitAddressess;

        if (force32BitAddressess) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
        }

        if (pDevice->areSharedSystemAllocationsAllowed() ||
            DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
        }

        if (DebugManager.flags.UseBindlessBuffers.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessBuffers);
        }

        if (DebugManager.flags.UseBindlessImages.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessImages);
        }

        kernelDebugEnabled = pDevice->isSourceLevelDebuggerActive();

        auto enableStatelessToStatefullWithOffset = pDevice->getHardwareCapabilities().isStatelesToStatefullWithOffsetSupported;
        if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
            enableStatelessToStatefullWithOffset = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
        }

        if (enableStatelessToStatefullWithOffset) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
        }
    }

    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::preserveVec3Type);
}

Program::~Program() {
    elfBinarySize = 0;

    cleanCurrentKernelInfo();

    freeBlockResources();

    delete blockKernelManager;

    if (constantSurface) {
        if ((nullptr != context) && (nullptr != context->getSVMAllocsManager()) && (context->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(constantSurface->getGpuAddress())))) {
            context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(constantSurface->getGpuAddress()));
        } else {
            this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(constantSurface);
        }
        constantSurface = nullptr;
    }

    if (globalSurface) {
        if ((nullptr != context) && (nullptr != context->getSVMAllocsManager()) && (context->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(globalSurface->getGpuAddress())))) {
            context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(globalSurface->getGpuAddress()));
        } else {
            this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(globalSurface);
        }
        globalSurface = nullptr;
    }

    if (context && !isBuiltIn) {
        context->decRefInternal();
    }
}

cl_int Program::createProgramFromBinary(
    const void *pBinary,
    size_t binarySize) {

    cl_int retVal = CL_INVALID_PROGRAM;
    uint32_t binaryVersion = iOpenCL::CURRENT_ICBE_VERSION;

    if (Program::isValidLlvmBinary(pBinary, binarySize)) {
        retVal = processSpirBinary(pBinary, binarySize, false);
    } else if (Program::isValidSpirvBinary(pBinary, binarySize)) {
        retVal = processSpirBinary(pBinary, binarySize, true);
    } else {
        bool rebuildRequired = DebugManager.flags.RebuildPrecompiledKernels.get();
        retVal = processElfBinary(pBinary, binarySize, binaryVersion);
        if (retVal == CL_SUCCESS) {
            isCreatedFromBinary = true;
        } else if (binaryVersion != iOpenCL::CURRENT_ICBE_VERSION) {
            // Version of compiler used to create program binary is invalid,
            // needs to recompile program binary from its IR (if available).
            // if recompile fails propagate error retVal from previous function
            rebuildRequired = true;
        }

        if (rebuildRequired) {
            if (rebuildProgramFromIr() == CL_SUCCESS) {
                retVal = CL_SUCCESS;
            }
        }
    }

    return retVal;
}

cl_int Program::rebuildProgramFromIr() {
    size_t dataSize;

    isSpirV = false;
    if (Program::isValidSpirvBinary(irBinary.get(), irBinarySize)) {
        isSpirV = true;
    } else if (false == Program::isValidLlvmBinary(irBinary.get(), irBinarySize)) {
        return CL_INVALID_PROGRAM;
    }

    CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

    elfWriter.addSection(CLElfLib::SSectionNode(isSpirV ? CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV : CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY,
                                                CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "", std::string(irBinary.get(), irBinarySize), static_cast<uint32_t>(irBinarySize)));

    dataSize = elfWriter.getTotalBinarySize();
    CLElfLib::ElfBinaryStorage data(dataSize);
    elfWriter.resolveBinary(data);

    CompilerInterface *pCompilerInterface = this->executionEnvironment.getCompilerInterface();
    if (nullptr == pCompilerInterface) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    TranslationInput inputArgs = {IGC::CodeType::elf, IGC::CodeType::oclGenBin};
    inputArgs.src = ArrayRef<const char>(data);
    inputArgs.apiOptions = ArrayRef<const char>(options);
    inputArgs.internalOptions = ArrayRef<const char>(internalOptions);

    TranslationOutput compilerOuput = {};
    auto err = pCompilerInterface->link(*this->pDevice, inputArgs, compilerOuput);
    this->updateBuildLog(this->pDevice, compilerOuput.frontendCompilerLog.c_str(), compilerOuput.frontendCompilerLog.size());
    this->updateBuildLog(this->pDevice, compilerOuput.backendCompilerLog.c_str(), compilerOuput.backendCompilerLog.size());
    if (TranslationOutput::ErrorCode::Success != err) {
        return asClError(err);
    }

    this->genBinary = std::move(compilerOuput.deviceBinary.mem);
    this->genBinarySize = compilerOuput.deviceBinary.size;
    this->debugData = std::move(compilerOuput.debugData.mem);
    this->debugDataSize = compilerOuput.debugData.size;

    auto retVal = processGenBinary();
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
    isCreatedFromBinary = true;
    isProgramBinaryResolved = true;

    return CL_SUCCESS;
}

cl_int Program::setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    if (!isSpirV) {
        return CL_INVALID_PROGRAM;
    }

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    if (!areSpecializationConstantsInitialized) {
        auto pCompilerInterface = this->executionEnvironment.getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        SpecConstantInfo specConstInfo;
        auto retVal = pCompilerInterface->getSpecConstantsInfo(this->getDevice(0), ArrayRef<const char>(sourceCode), specConstInfo);

        if (retVal != TranslationOutput::ErrorCode::Success) {
            return CL_INVALID_VALUE;
        }

        areSpecializationConstantsInitialized = true;
    }

    return updateSpecializationConstant(specId, specSize, specValue);
}

cl_int Program::updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    for (uint32_t i = 0; i < specConstantsIds->GetSize<cl_uint>(); i++) {
        if (specConstantsIds->GetMemory<cl_uint>()[i] == specId) {
            if (specConstantsSizes->GetMemory<size_t>()[i] == specSize) {
                specConstantsValues->GetMemoryWriteable<const void *>()[i] = specValue;
                return CL_SUCCESS;
            } else {
                return CL_INVALID_VALUE;
            }
        }
    }
    return CL_INVALID_SPEC_ID;
}

bool Program::isValidLlvmBinary(
    const void *pBinary,
    size_t binarySize) {

    const char *pLlvmMagic = "BC\xc0\xde";
    bool retVal = false;

    if (pBinary && (binarySize > (strlen(pLlvmMagic) + 1))) {
        if (strstr((char *)pBinary, pLlvmMagic) != nullptr) {
            retVal = true;
        }
    }

    return retVal;
}

cl_int Program::getSource(std::string &binary) const {
    cl_int retVal = CL_INVALID_PROGRAM;
    binary = {};
    if (!sourceCode.empty()) {
        binary = sourceCode;
        retVal = CL_SUCCESS;
    }
    return retVal;
}

void Program::updateBuildLog(const Device *pDevice, const char *pErrorString,
                             size_t errorStringSize) {
    if ((pErrorString == nullptr) || (errorStringSize == 0) || (pErrorString[0] == '\0')) {
        return;
    }

    if (pErrorString[errorStringSize - 1] == '\0') {
        --errorStringSize;
    }

    auto it = buildLog.find(pDevice);

    if (it == buildLog.end()) {
        buildLog[pDevice].assign(pErrorString, pErrorString + errorStringSize);
        return;
    }

    buildLog[pDevice].append("\n");
    buildLog[pDevice].append(pErrorString, pErrorString + errorStringSize);
}

const char *Program::getBuildLog(const Device *pDevice) const {
    const char *entry = nullptr;

    auto it = buildLog.find(pDevice);

    if (it != buildLog.end()) {
        entry = it->second.c_str();
    }

    return entry;
}

void Program::separateBlockKernels() {
    if ((0 == parentKernelInfoArray.size()) && (0 == subgroupKernelInfoArray.size())) {
        return;
    }

    auto allKernelInfos(kernelInfoArray);
    kernelInfoArray.clear();
    for (auto &i : allKernelInfos) {
        auto end = i->name.rfind("_dispatch_");
        if (end != std::string::npos) {
            bool baseKernelFound = false;
            std::string baseKernelName(i->name, 0, end);
            for (auto &j : parentKernelInfoArray) {
                if (j->name.compare(baseKernelName) == 0) {
                    baseKernelFound = true;
                    break;
                }
            }
            if (!baseKernelFound) {
                for (auto &j : subgroupKernelInfoArray) {
                    if (j->name.compare(baseKernelName) == 0) {
                        baseKernelFound = true;
                        break;
                    }
                }
            }
            if (baseKernelFound) {
                //Parent or subgroup kernel found -> child kernel
                blockKernelManager->addBlockKernelInfo(i);
            } else {
                kernelInfoArray.push_back(i);
            }
        } else {
            //Regular kernel found
            kernelInfoArray.push_back(i);
        }
    }
    allKernelInfos.clear();
}

void Program::allocateBlockPrivateSurfaces(uint32_t rootDeviceIndex) {
    size_t blockCount = blockKernelManager->getCount();

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *info = blockKernelManager->getBlockKernelInfo(i);

        if (info->patchInfo.pAllocateStatelessPrivateSurface) {
            size_t privateSize = info->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize;

            if (privateSize > 0 && blockKernelManager->getPrivateSurface(i) == nullptr) {
                privateSize *= getDevice(0).getDeviceInfo().computeUnitsUsedForScratch * info->getMaxSimdSize();
                auto *privateSurface = this->executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, privateSize, GraphicsAllocation::AllocationType::PRIVATE_SURFACE});
                blockKernelManager->pushPrivateSurface(privateSurface, i);
            }
        }
    }
}

void Program::freeBlockResources() {
    size_t blockCount = blockKernelManager->getCount();

    for (uint32_t i = 0; i < blockCount; i++) {

        auto *privateSurface = blockKernelManager->getPrivateSurface(i);

        if (privateSurface != nullptr) {
            blockKernelManager->pushPrivateSurface(nullptr, i);
            this->executionEnvironment.memoryManager->freeGraphicsMemory(privateSurface);
        }
        auto kernelInfo = blockKernelManager->getBlockKernelInfo(i);
        DEBUG_BREAK_IF(!kernelInfo->kernelAllocation);
        if (kernelInfo->kernelAllocation) {
            this->executionEnvironment.memoryManager->freeGraphicsMemory(kernelInfo->kernelAllocation);
        }
    }
}

void Program::cleanCurrentKernelInfo() {
    for (auto &kernelInfo : kernelInfoArray) {
        if (kernelInfo->kernelAllocation) {
            //register cache flush in all csrs where kernel allocation was used
            for (auto &engine : this->executionEnvironment.memoryManager->getRegisteredEngines()) {
                auto contextId = engine.osContext->getContextId();
                if (kernelInfo->kernelAllocation->isUsedByOsContext(contextId)) {
                    engine.commandStreamReceiver->registerInstructionCacheFlush();
                }
            }

            this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(kernelInfo->kernelAllocation);
        }
        delete kernelInfo;
    }
    kernelInfoArray.clear();
}

void Program::updateNonUniformFlag() {
    //Look for -cl-std=CL substring and extract value behind which can be 1.2 2.0 2.1 and convert to value
    auto pos = options.find(clOptNameClVer);
    if (pos == std::string::npos) {
        programOptionVersion = 12u; //Default is 1.2
    } else {
        std::stringstream ss{options.c_str() + pos + clOptNameClVer.size()};
        uint32_t majorV = 0u, minorV = 0u;
        char dot = 0u;
        ss >> majorV;
        ss >> dot;
        ss >> minorV;
        programOptionVersion = majorV * 10u + minorV;
    }

    if (programOptionVersion >= 20u && (false == CompilerOptions::contains(options, CompilerOptions::uniformWorkgroupSize))) {
        allowNonUniform = true;
    }
}

void Program::updateNonUniformFlag(const Program **inputPrograms, size_t numInputPrograms) {
    bool allowNonUniform = true;
    for (cl_uint i = 0; i < numInputPrograms; i++) {
        allowNonUniform = allowNonUniform && inputPrograms[i]->getAllowNonUniform();
    }
    this->allowNonUniform = allowNonUniform;
}

void Program::prepareLinkerInputStorage() {
    if (this->linkerInput == nullptr) {
        this->linkerInput = std::make_unique<LinkerInput>();
    }
}
} // namespace NEO

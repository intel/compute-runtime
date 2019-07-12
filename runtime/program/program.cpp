/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

#include "elf/writer.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/context/context.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/unified_memory_manager.h"

#include <sstream>

namespace NEO {

const std::string Program::clOptNameClVer("-cl-std=CL");
const std::string Program::clOptNameUniformWgs{"-cl-uniform-work-group-size"};

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
    programScopePatchListSize = 0;
    programScopePatchList = nullptr;
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
            internalOptions += "-m32 ";
        }

        if (DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
            internalOptions += "-cl-intel-greater-than-4GB-buffer-required ";
        }
        kernelDebugEnabled = pDevice->isSourceLevelDebuggerActive();

        auto enableStatelessToStatefullWithOffset = pDevice->getHardwareCapabilities().isStatelesToStatefullWithOffsetSupported;
        if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
            enableStatelessToStatefullWithOffset = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
        }

        if (enableStatelessToStatefullWithOffset) {
            internalOptions += "-cl-intel-has-buffer-offset-arg ";
        }
    }

    internalOptions += "-fpreserve-vec3-type ";
}

Program::~Program() {
    delete[] genBinary;
    genBinary = nullptr;

    delete[] irBinary;
    irBinary = nullptr;

    delete[] debugData;
    debugData = nullptr;

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
    cl_int retVal = CL_SUCCESS;
    size_t dataSize;

    do {
        if (!Program::isValidLlvmBinary(irBinary, irBinarySize)) {
            if ((!Program::isValidSpirvBinary(irBinary, irBinarySize))) {
                retVal = CL_INVALID_PROGRAM;
                break;
            }
            isSpirV = true;
        }

        CLElfLib::CElfWriter elfWriter(CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_OBJECTS, CLElfLib::E_EH_MACHINE::EH_MACHINE_NONE, 0);

        elfWriter.addSection(CLElfLib::SSectionNode(isSpirV ? CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV : CLElfLib::E_SH_TYPE::SH_TYPE_OPENCL_LLVM_BINARY,
                                                    CLElfLib::E_SH_FLAG::SH_FLAG_NONE, "", std::string(irBinary, irBinarySize), static_cast<uint32_t>(irBinarySize)));

        dataSize = elfWriter.getTotalBinarySize();
        CLElfLib::ElfBinaryStorage data(dataSize);
        elfWriter.resolveBinary(data);

        CompilerInterface *pCompilerInterface = this->executionEnvironment.getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationArgs inputArgs = {};
        inputArgs.pInput = data.data();
        inputArgs.InputSize = static_cast<uint32_t>(dataSize);
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = static_cast<uint32_t>(options.length());
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = static_cast<uint32_t>(internalOptions.length());
        inputArgs.pTracingOptions = nullptr;
        inputArgs.TracingOptionsCount = 0;

        retVal = pCompilerInterface->link(*this, inputArgs);
        if (retVal != CL_SUCCESS) {
            break;
        }

        retVal = processGenBinary();
        if (retVal != CL_SUCCESS) {
            break;
        }

        programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        isCreatedFromBinary = true;
        isProgramBinaryResolved = true;
    } while (false);

    return retVal;
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

        TranslationArgs inputArgs = {};
        inputArgs.pInput = const_cast<char *>(sourceCode.c_str());
        inputArgs.InputSize = static_cast<uint32_t>(sourceCode.size());
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = static_cast<uint32_t>(options.length());
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = static_cast<uint32_t>(internalOptions.length());
        inputArgs.pTracingOptions = nullptr;
        inputArgs.TracingOptionsCount = 0;

        auto retVal = pCompilerInterface->getSpecConstantsInfo(*this, inputArgs);

        if (retVal != CL_SUCCESS) {
            return retVal;
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

void Program::getProgramCompilerVersion(
    SProgramBinaryHeader *pSectionData,
    uint32_t &binaryVersion) const {
    if (pSectionData != nullptr) {
        binaryVersion = pSectionData->Version;
    }
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

void Program::setSource(const char *pSourceString) {
    sourceCode = pSourceString;
}

cl_int Program::getSource(char *&pBinary, unsigned int &dataSize) const {
    cl_int retVal = CL_INVALID_PROGRAM;
    pBinary = nullptr;
    dataSize = 0;
    if (!sourceCode.empty()) {
        pBinary = (char *)(sourceCode.c_str());
        dataSize = (unsigned int)(sourceCode.size());
        retVal = CL_SUCCESS;
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

void Program::storeGenBinary(
    const void *pSrc,
    const size_t srcSize) {
    storeBinary(genBinary, genBinarySize, pSrc, srcSize);
}

void Program::storeIrBinary(
    const void *pSrc,
    const size_t srcSize,
    bool isSpirV) {
    storeBinary(irBinary, irBinarySize, pSrc, srcSize);
    this->isSpirV = isSpirV;
}

void Program::storeDebugData(
    const void *pSrc,
    const size_t srcSize) {
    storeBinary(debugData, debugDataSize, pSrc, srcSize);
}

void Program::storeBinary(
    char *&pDst,
    size_t &dstSize,
    const void *pSrc,
    const size_t srcSize) {
    dstSize = 0;

    DEBUG_BREAK_IF(!(pSrc && srcSize > 0));

    delete[] pDst;
    pDst = new char[srcSize];

    dstSize = (cl_uint)srcSize;
    memcpy_s(pDst, dstSize, pSrc, srcSize);
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

void Program::allocateBlockPrivateSurfaces() {
    size_t blockCount = blockKernelManager->getCount();

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *info = blockKernelManager->getBlockKernelInfo(i);

        if (info->patchInfo.pAllocateStatelessPrivateSurface) {
            size_t privateSize = info->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize;

            if (privateSize > 0 && blockKernelManager->getPrivateSurface(i) == nullptr) {
                privateSize *= getDevice(0).getDeviceInfo().computeUnitsUsedForScratch * info->getMaxSimdSize();
                auto *privateSurface = this->executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties({privateSize, GraphicsAllocation::AllocationType::PRIVATE_SURFACE});
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

    if (programOptionVersion >= 20u && options.find(clOptNameUniformWgs) == std::string::npos) {
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

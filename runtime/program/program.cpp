/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "program.h"
#include "elf/writer.h"
#include "runtime/context/context.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/compiler_interface/compiler_interface.h"

#include <sstream>

namespace OCLRT {

const std::string Program::clOptNameClVer("-cl-std=CL");
const std::string Program::clOptNameUniformWgs{"-cl-uniform-work-group-size"};

Program::Program() : Program(nullptr, false) {
    numDevices = 0;
}

Program::Program(Context *context, bool isBuiltIn) : context(context), isBuiltIn(isBuiltIn) {
    if (this->context && !this->isBuiltIn) {
        this->context->incRefInternal();
    }
    blockKernelManager = new BlockKernelManager();
    pDevice = context ? context->getDevice(0) : nullptr;
    numDevices = 1;
    elfBinary = nullptr;
    elfBinarySize = 0;
    genBinary = nullptr;
    genBinarySize = 0;
    llvmBinary = nullptr;
    llvmBinarySize = 0;
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
        pDevice->increaseProgramCount();

        if (DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
            internalOptions += "-cl-intel-greater-than-4GB-buffer-required ";
        }
        kernelDebugEnabled = pDevice->isSourceLevelDebuggerActive();
    }

    if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get()) {
        internalOptions += "-cl-intel-has-buffer-offset-arg ";
    }

    internalOptions += "-fpreserve-vec3-type ";
}

Program::~Program() {
    if (context && !isBuiltIn) {
        context->decRefInternal();
    }
    delete[] genBinary;
    genBinary = nullptr;

    delete[] llvmBinary;
    llvmBinary = nullptr;

    delete[] debugData;
    debugData = nullptr;

    delete[] elfBinary;
    elfBinary = nullptr;
    elfBinarySize = 0;

    cleanCurrentKernelInfo();

    freeBlockResources();

    delete blockKernelManager;

    if (constantSurface) {
        pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(constantSurface);
        constantSurface = nullptr;
    }

    if (globalSurface) {
        pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(globalSurface);
        globalSurface = nullptr;
    }
}

cl_int Program::createProgramFromBinary(
    const void *pBinary,
    size_t binarySize) {

    cl_int retVal = CL_SUCCESS;
    uint32_t binaryVersion = iOpenCL::CURRENT_ICBE_VERSION;

    if (Program::isValidLlvmBinary(pBinary, binarySize)) {
        retVal = processSpirBinary(pBinary, binarySize, false);
    } else if (Program::isValidSpirvBinary(pBinary, binarySize)) {
        retVal = processSpirBinary(pBinary, binarySize, true);
    } else {
        retVal = processElfBinary(pBinary, binarySize, binaryVersion);
        if (retVal == CL_SUCCESS) {
            isCreatedFromBinary = true;
        } else if (binaryVersion != iOpenCL::CURRENT_ICBE_VERSION) {
            // Version of compiler used to create program binary is invalid,
            // needs to recompile program binary from its LLVM (if available).
            // if recompile fails propagate error retVal from previous function
            if (!rebuildProgramFromLLVM()) {
                retVal = CL_SUCCESS;
            }
        }
    }

    return retVal;
}

cl_int Program::rebuildProgramFromLLVM() {
    cl_int retVal = CL_SUCCESS;
    size_t dataSize;
    char *pData = nullptr;
    CLElfLib::CElfWriter *pElfWriter = nullptr;

    do {
        if (!Program::isValidLlvmBinary(llvmBinary, llvmBinarySize)) {
            retVal = CL_INVALID_PROGRAM;
            break;
        }

        pElfWriter = CLElfLib::CElfWriter::create(CLElfLib::EH_TYPE_OPENCL_OBJECTS, CLElfLib::EH_MACHINE_NONE, 0);

        CLElfLib::SSectionNode sectionNode;
        sectionNode.Name = "";
        sectionNode.Type = CLElfLib::SH_TYPE_OPENCL_LLVM_BINARY;
        sectionNode.Flags = 0;
        sectionNode.pData = llvmBinary;
        sectionNode.DataSize = static_cast<unsigned int>(llvmBinarySize);
        pElfWriter->addSection(&sectionNode);

        pElfWriter->resolveBinary(nullptr, dataSize);
        pData = new char[dataSize];
        pElfWriter->resolveBinary(pData, dataSize);

        CompilerInterface *pCompilerInterface = getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            retVal = CL_OUT_OF_HOST_MEMORY;
            break;
        }

        TranslationArgs inputArgs = {};
        inputArgs.pInput = pData;
        inputArgs.InputSize = static_cast<unsigned int>(dataSize);
        inputArgs.pOptions = options.c_str();
        inputArgs.OptionsSize = static_cast<unsigned int>(options.length());
        inputArgs.pInternalOptions = internalOptions.c_str();
        inputArgs.InternalOptionsSize = static_cast<unsigned int>(internalOptions.length());
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

    CLElfLib::CElfWriter::destroy(pElfWriter);
    delete[] pData;

    return retVal;
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

void Program::setSource(char *pSourceString) {
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

void Program::storeGenBinary(
    const void *pSrc,
    const size_t srcSize) {
    storeBinary(genBinary, genBinarySize, pSrc, srcSize);
}

void Program::storeLlvmBinary(
    const void *pSrc,
    const size_t srcSize) {
    storeBinary(llvmBinary, llvmBinarySize, pSrc, srcSize);
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

CompilerInterface *Program::getCompilerInterface() const {
    return CompilerInterface::getInstance();
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

                auto *privateSurface = getDevice(0).getMemoryManager()->createGraphicsAllocationWithRequiredBitness(privateSize, nullptr);
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
            getDevice(0).getMemoryManager()->freeGraphicsMemory(privateSurface);
        }
        auto kernelInfo = blockKernelManager->getBlockKernelInfo(i);
        DEBUG_BREAK_IF(!kernelInfo->kernelAllocation);
        if (kernelInfo->kernelAllocation) {
            getDevice(0).getMemoryManager()->freeGraphicsMemory(kernelInfo->kernelAllocation);
        }
    }
}

void Program::cleanCurrentKernelInfo() {
    for (auto &kernelInfo : kernelInfoArray) {
        if (kernelInfo->kernelAllocation) {
            this->pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(kernelInfo->kernelAllocation);
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
        uint32_t majorV, minorV;
        char dot;
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
} // namespace OCLRT

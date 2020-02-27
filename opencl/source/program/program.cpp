/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/context/context.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/program/kernel_info.h"

#include "compiler_options.h"

#include <sstream>

namespace NEO {

const std::string Program::clOptNameClVer("-cl-std=CL");

Program::Program(ExecutionEnvironment &executionEnvironment) : Program(executionEnvironment, nullptr, false, nullptr) {
    numDevices = 0;
}

Program::Program(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn, Device *device) : executionEnvironment(executionEnvironment),
                                                                                                                 context(context),
                                                                                                                 pDevice(device),
                                                                                                                 isBuiltIn(isBuiltIn) {
    if (this->context && !this->isBuiltIn) {
        this->context->incRefInternal();
    }
    blockKernelManager = new BlockKernelManager();
    auto clDevice = context != nullptr ? context->getDevice(0) : (device != nullptr) ? device->getSpecializedDevice<ClDevice>() : nullptr;
    if (pDevice == nullptr && context != nullptr) {
        pDevice = &context->getDevice(0)->getDevice();
    }
    numDevices = 1;
    char paramValue[32] = {};
    bool force32BitAddressess = false;

    if (clDevice) {
        clDevice->getDeviceInfo(CL_DEVICE_VERSION, 32, paramValue, nullptr);
        if (strstr(paramValue, "2.1")) {
            internalOptions = "-ocl-version=210 ";
        } else if (strstr(paramValue, "2.0")) {
            internalOptions = "-ocl-version=200 ";
        } else if (strstr(paramValue, "1.2")) {
            internalOptions = "-ocl-version=120 ";
        }
        force32BitAddressess = clDevice->getDeviceInfo().force32BitAddressess;

        if (force32BitAddressess) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
        }

        if (clDevice->areSharedSystemAllocationsAllowed() ||
            DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
        }

        if (DebugManager.flags.UseBindlessBuffers.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessBuffers);
        }

        if (DebugManager.flags.UseBindlessImages.get()) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessImages);
        }

        kernelDebugEnabled = clDevice->isDebuggerActive();

        auto enableStatelessToStatefullWithOffset = clDevice->getHardwareCapabilities().isStatelesToStatefullWithOffsetSupported;
        if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
            enableStatelessToStatefullWithOffset = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
        }

        if (enableStatelessToStatefullWithOffset) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
        }

        auto &hwHelper = HwHelper::get(clDevice->getHardwareInfo().platform.eRenderCoreFamily);
        if (hwHelper.isForceEmuInt32DivRemSPWARequired(clDevice->getHardwareInfo())) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::forceEmuInt32DivRemSP);
        }
    }

    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::preserveVec3Type);
}

Program::~Program() {
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

    if (specConstantsValues.get() != nullptr) {
        for (auto i = 0u; i < specConstantsValues->GetSize<void *>(); i++) {
            auto specConstPtr = specConstantsValues->GetMemory<void *>()[i];
            if (specConstPtr != nullptr) {
                delete[] reinterpret_cast<char *>(specConstPtr);
            }
        }
    }
}

cl_int Program::createProgramFromBinary(
    const void *pBinary,
    size_t binarySize) {

    cl_int retVal = CL_INVALID_BINARY;

    this->irBinary.reset();
    this->irBinarySize = 0U;
    this->isSpirV = false;
    this->unpackedDeviceBinary.reset();
    this->unpackedDeviceBinarySize = 0U;
    this->packedDeviceBinary.reset();
    this->packedDeviceBinarySize = 0U;
    this->createdFrom = CreatedFrom::BINARY;

    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(pBinary), binarySize);
    bool isSpirV = NEO::isSpirVBitcode(archive);

    if (isSpirV || NEO::isLlvmBitcode(archive)) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;
        retVal = processSpirBinary(archive.begin(), archive.size(), isSpirV);
    } else if (isAnyDeviceBinaryFormat(archive)) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        this->isCreatedFromBinary = true;

        auto productAbbreviation = hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily];

        TargetDevice targetDevice = {};
        targetDevice.coreFamily = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
        targetDevice.stepping = pDevice->getHardwareInfo().platform.usRevId;
        targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
        std::string decodeErrors;
        std::string decodeWarnings;
        auto singleDeviceBinary = unpackSingleDeviceBinary(archive, ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                           decodeErrors, decodeWarnings);
        if (decodeWarnings.empty() == false) {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
        }

        if (singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty()) {
            retVal = CL_INVALID_BINARY;
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        } else {
            retVal = CL_SUCCESS;
            this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
            this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
            this->isSpirV = NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize));
            this->options = singleDeviceBinary.buildOptions.str();

            if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == DebugManager.flags.RebuildPrecompiledKernels.get())) {
                this->unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
                this->unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
                this->packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
                this->packedDeviceBinarySize = archive.size();
            } else {
                this->isCreatedFromBinary = false;
            }

            switch (singleDeviceBinary.format) {
            default:
                break;
            case DeviceBinaryFormat::OclLibrary:
                this->programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
                break;
            case DeviceBinaryFormat::OclCompiledObject:
                this->programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
                break;
            }
        }
    }

    return retVal;
}

cl_int Program::setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    if (!isSpirV) {
        return CL_INVALID_PROGRAM;
    }

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    if (!areSpecializationConstantsInitialized) {
        auto pCompilerInterface = this->pDevice->getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        SpecConstantInfo specConstInfo;
        auto retVal = pCompilerInterface->getSpecConstantsInfo(this->getDevice(), ArrayRef<const char>(sourceCode), specConstInfo);

        if (retVal != TranslationOutput::ErrorCode::Success) {
            return CL_INVALID_VALUE;
        }

        this->specConstantsIds.reset(specConstInfo.idsBuffer.release());
        this->specConstantsSizes.reset(specConstInfo.sizesBuffer.release());
        this->specConstantsValues.reset(specConstInfo.valuesBuffer.release());

        areSpecializationConstantsInitialized = true;
    }

    return updateSpecializationConstant(specId, specSize, specValue);
}

cl_int Program::updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    for (uint32_t i = 0; i < specConstantsIds->GetSize<uint32_t>(); i++) {
        if (specConstantsIds->GetMemory<uint32_t>()[i] == specId) {
            if (specConstantsSizes->GetMemory<uint32_t>()[i] == static_cast<uint32_t>(specSize)) {
                auto specConstPtr = specConstantsValues->GetMemoryWriteable<void *>()[i];
                if (specConstPtr == nullptr) {
                    specConstPtr = new char[specSize];
                }
                memcpy_s(specConstPtr, specSize, specValue, specSize);
                specConstantsValues->GetMemoryWriteable<void *>()[i] = specConstPtr;
                return CL_SUCCESS;
            } else {
                return CL_INVALID_VALUE;
            }
        }
    }
    return CL_INVALID_SPEC_ID;
}

void Program::setDevice(Device *device) {
    this->pDevice = device;
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
                privateSize *= getDevice().getDeviceInfo().computeUnitsUsedForScratch * info->getMaxSimdSize();
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

void Program::replaceDeviceBinary(std::unique_ptr<char[]> newBinary, size_t newBinarySize) {
    if (isAnyPackedDeviceBinaryFormat(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(newBinary.get()), newBinarySize))) {
        this->packedDeviceBinary = std::move(newBinary);
        this->packedDeviceBinarySize = newBinarySize;
        this->unpackedDeviceBinary.reset();
        this->unpackedDeviceBinarySize = 0U;
    } else {
        this->packedDeviceBinary.reset();
        this->packedDeviceBinarySize = 0U;
        this->unpackedDeviceBinary = std::move(newBinary);
        this->unpackedDeviceBinarySize = newBinarySize;
    }
}

cl_int Program::packDeviceBinary() {
    if (nullptr != packedDeviceBinary) {
        return CL_SUCCESS;
    }

    auto gfxCore = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    auto stepping = pDevice->getHardwareInfo().platform.usRevId;

    if (nullptr != this->unpackedDeviceBinary.get()) {
        SingleDeviceBinary singleDeviceBinary;
        singleDeviceBinary.buildOptions = this->options;
        singleDeviceBinary.targetDevice.coreFamily = gfxCore;
        singleDeviceBinary.targetDevice.stepping = stepping;
        singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
        singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize);
        std::string packWarnings;
        std::string packErrors;
        auto packedDeviceBinary = NEO::packDeviceBinary(singleDeviceBinary, packErrors, packWarnings);
        if (packedDeviceBinary.empty()) {
            DEBUG_BREAK_IF(true);
            return CL_OUT_OF_HOST_MEMORY;
        }
        this->packedDeviceBinary = makeCopy(packedDeviceBinary.data(), packedDeviceBinary.size());
        this->packedDeviceBinarySize = packedDeviceBinary.size();
    } else if (nullptr != this->irBinary.get()) {
        NEO::Elf::ElfEncoder<> elfEncoder(true, true, 1U);
        if (this->programBinaryType == CL_PROGRAM_BINARY_TYPE_LIBRARY) {
            elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
        } else {
            elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;
        }
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(this->irBinary.get(), this->irBinarySize));
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions, this->options);
        auto elfData = elfEncoder.encode();
        this->packedDeviceBinary = makeCopy(elfData.data(), elfData.size());
        this->packedDeviceBinarySize = elfData.size();
    } else {
        return CL_INVALID_PROGRAM;
    }

    return CL_SUCCESS;
}

} // namespace NEO

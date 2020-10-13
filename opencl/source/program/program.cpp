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
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/program/kernel_info.h"

#include "compiler_options.h"

#include <sstream>

namespace NEO {

Program::Program(Context *context, bool isBuiltIn, const ClDeviceVector &clDevicesIn) : executionEnvironment(*clDevicesIn[0]->getExecutionEnvironment()),
                                                                                        context(context),
                                                                                        pDevice(&clDevicesIn[0]->getDevice()),
                                                                                        clDevices(clDevicesIn),
                                                                                        isBuiltIn(isBuiltIn) {
    if (this->context && !this->isBuiltIn) {
        this->context->incRefInternal();
    }
    blockKernelManager = new BlockKernelManager();
    ClDevice *pClDevice = castToObject<ClDevice>(pDevice->getSpecializedDevice<ClDevice>());

    numDevices = static_cast<uint32_t>(clDevicesIn.size());
    bool force32BitAddressess = false;

    uint32_t maxRootDeviceIndex = 0;

    for (const auto &device : clDevicesIn) {
        if (device->getRootDeviceIndex() > maxRootDeviceIndex) {
            maxRootDeviceIndex = device->getRootDeviceIndex();
        }
    }

    buildInfos.resize(maxRootDeviceIndex + 1);

    auto enabledClVersion = pClDevice->getEnabledClVersion();
    if (enabledClVersion == 30) {
        internalOptions = "-ocl-version=300 ";
    } else if (enabledClVersion == 21) {
        internalOptions = "-ocl-version=210 ";
    } else {
        internalOptions = "-ocl-version=120 ";
    }
    force32BitAddressess = pClDevice->getSharedDeviceInfo().force32BitAddressess;

    if (force32BitAddressess && !isBuiltIn) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
    }

    if ((isBuiltIn && is32bit) || pClDevice->areSharedSystemAllocationsAllowed() ||
        DebugManager.flags.DisableStatelessToStatefulOptimization.get()) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
    }

    if (ApiSpecificConfig::getBindlessConfiguration()) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessBuffers);
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessImages);
    }

    kernelDebugEnabled = pClDevice->isDebuggerActive();

    auto enableStatelessToStatefullWithOffset = pClDevice->getHardwareCapabilities().isStatelesToStatefullWithOffsetSupported;
    if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
        enableStatelessToStatefullWithOffset = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
    }

    if (enableStatelessToStatefullWithOffset) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
    }

    auto &hwHelper = HwHelper::get(pClDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.isForceEmuInt32DivRemSPWARequired(pClDevice->getHardwareInfo())) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::forceEmuInt32DivRemSP);
    }

    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::preserveVec3Type);
}

Program::~Program() {
    cleanCurrentKernelInfo();

    freeBlockResources();

    delete blockKernelManager;
    for (const auto &buildInfo : buildInfos) {
        if (buildInfo.constantSurface) {
            if ((nullptr != context) && (nullptr != context->getSVMAllocsManager()) && (context->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(buildInfo.constantSurface->getGpuAddress())))) {
                context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(buildInfo.constantSurface->getGpuAddress()));
            } else {
                this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(buildInfo.constantSurface);
            }
        }

        if (buildInfo.globalSurface) {
            if ((nullptr != context) && (nullptr != context->getSVMAllocsManager()) && (context->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(buildInfo.globalSurface->getGpuAddress())))) {
                context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(buildInfo.globalSurface->getGpuAddress()));
            } else {
                this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(buildInfo.globalSurface);
            }
        }
    }

    if (context && !isBuiltIn) {
        context->decRefInternal();
    }
}

cl_int Program::createProgramFromBinary(
    const void *pBinary,
    size_t binarySize, uint32_t rootDeviceIndex) {

    cl_int retVal = CL_INVALID_BINARY;

    this->irBinary.reset();
    this->irBinarySize = 0U;
    this->isSpirV = false;
    this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset();
    this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = 0U;
    this->buildInfos[rootDeviceIndex].packedDeviceBinary.reset();
    this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = 0U;
    this->createdFrom = CreatedFrom::BINARY;

    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(pBinary), binarySize);
    bool isSpirV = NEO::isSpirVBitcode(archive);

    if (isSpirV || NEO::isLlvmBitcode(archive)) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;
        retVal = processSpirBinary(archive.begin(), archive.size(), isSpirV);
    } else if (isAnyDeviceBinaryFormat(archive)) {
        this->programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        this->isCreatedFromBinary = true;

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
        auto productAbbreviation = hardwarePrefix[hwInfo->platform.eProductFamily];

        TargetDevice targetDevice = {};
        targetDevice.coreFamily = hwInfo->platform.eRenderCoreFamily;
        targetDevice.stepping = hwInfo->platform.usRevId;
        targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
        std::string decodeErrors;
        std::string decodeWarnings;
        auto singleDeviceBinary = unpackSingleDeviceBinary(archive, ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                           decodeErrors, decodeWarnings);
        if (decodeWarnings.empty() == false) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
        }

        if (singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty()) {
            retVal = CL_INVALID_BINARY;
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        } else {
            retVal = CL_SUCCESS;
            this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
            this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
            this->isSpirV = NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize));
            this->options = singleDeviceBinary.buildOptions.str();

            if (false == singleDeviceBinary.debugData.empty()) {
                this->debugData = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
                this->debugDataSize = singleDeviceBinary.debugData.size();
            }

            if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == DebugManager.flags.RebuildPrecompiledKernels.get())) {
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
                this->buildInfos[rootDeviceIndex].packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
                this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = archive.size();
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
        auto retVal = pCompilerInterface->getSpecConstantsInfo(this->getDevice(), ArrayRef<const char>(irBinary.get(), irBinarySize), specConstInfo);

        if (retVal != TranslationOutput::ErrorCode::Success) {
            return CL_INVALID_VALUE;
        }

        this->specConstantsIds.reset(specConstInfo.idsBuffer.release());
        this->specConstantsSizes.reset(specConstInfo.sizesBuffer.release());

        areSpecializationConstantsInitialized = true;
    }

    return updateSpecializationConstant(specId, specSize, specValue);
}

cl_int Program::updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    for (uint32_t i = 0; i < specConstantsIds->GetSize<uint32_t>(); i++) {
        if (specConstantsIds->GetMemory<uint32_t>()[i] == specId) {
            if (specConstantsSizes->GetMemory<uint32_t>()[i] == static_cast<uint32_t>(specSize)) {
                uint64_t specConstValue = 0u;
                memcpy_s(&specConstValue, sizeof(uint64_t), specValue, specSize);
                specConstantsValues[specId] = specConstValue;
                return CL_SUCCESS;
            } else {
                return CL_INVALID_VALUE;
            }
        }
    }
    return CL_INVALID_SPEC_ID;
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

void Program::updateBuildLog(uint32_t rootDeviceIndex, const char *pErrorString,
                             size_t errorStringSize) {
    if ((pErrorString == nullptr) || (errorStringSize == 0) || (pErrorString[0] == '\0')) {
        return;
    }

    if (pErrorString[errorStringSize - 1] == '\0') {
        --errorStringSize;
    }

    auto &currentLog = buildInfos[rootDeviceIndex].buildLog;

    if (currentLog.empty()) {
        currentLog.assign(pErrorString, pErrorString + errorStringSize);
        return;
    }

    currentLog.append("\n");
    currentLog.append(pErrorString, pErrorString + errorStringSize);
}

const char *Program::getBuildLog(uint32_t rootDeviceIndex) const {
    auto &currentLog = buildInfos[rootDeviceIndex].buildLog;
    return currentLog.c_str();
}

void Program::separateBlockKernels() {
    if ((0 == parentKernelInfoArray.size()) && (0 == subgroupKernelInfoArray.size())) {
        return;
    }

    auto allKernelInfos(kernelInfoArray);
    kernelInfoArray.clear();
    for (auto &i : allKernelInfos) {
        auto end = i->kernelDescriptor.kernelMetadata.kernelName.rfind("_dispatch_");
        if (end != std::string::npos) {
            bool baseKernelFound = false;
            std::string baseKernelName(i->kernelDescriptor.kernelMetadata.kernelName, 0, end);
            for (auto &j : parentKernelInfoArray) {
                if (j->kernelDescriptor.kernelMetadata.kernelName.compare(baseKernelName) == 0) {
                    baseKernelFound = true;
                    break;
                }
            }
            if (!baseKernelFound) {
                for (auto &j : subgroupKernelInfoArray) {
                    if (j->kernelDescriptor.kernelMetadata.kernelName.compare(baseKernelName) == 0) {
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
            auto perThreadPrivateMemorySize = info->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize;

            if (perThreadPrivateMemorySize > 0 && blockKernelManager->getPrivateSurface(i) == nullptr) {
                auto privateSize = static_cast<size_t>(KernelHelper::getPrivateSurfaceSize(perThreadPrivateMemorySize, getDevice().getDeviceInfo().computeUnitsUsedForScratch,
                                                                                           info->getMaxSimdSize(), info->patchInfo.pAllocateStatelessPrivateSurface->IsSimtThread));

                auto *privateSurface = this->executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, privateSize, GraphicsAllocation::AllocationType::PRIVATE_SURFACE, getDevice().getDeviceBitfield()});
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
    auto pos = options.find(clStdOptionName);
    if (pos == std::string::npos) {
        programOptionVersion = 12u; //Default is 1.2
    } else {
        std::stringstream ss{options.c_str() + pos + clStdOptionName.size()};
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

void Program::replaceDeviceBinary(std::unique_ptr<char[]> newBinary, size_t newBinarySize, uint32_t rootDeviceIndex) {
    if (isAnyPackedDeviceBinaryFormat(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(newBinary.get()), newBinarySize))) {
        this->buildInfos[rootDeviceIndex].packedDeviceBinary = std::move(newBinary);
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = newBinarySize;
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset();
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = 0U;
        if (isAnySingleDeviceBinaryFormat(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(this->buildInfos[rootDeviceIndex].packedDeviceBinary.get()), this->buildInfos[rootDeviceIndex].packedDeviceBinarySize))) {
            this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(buildInfos[rootDeviceIndex].packedDeviceBinary.get(), buildInfos[rootDeviceIndex].packedDeviceBinarySize);
            this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = buildInfos[rootDeviceIndex].packedDeviceBinarySize;
        }
    } else {
        this->buildInfos[rootDeviceIndex].packedDeviceBinary.reset();
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = 0U;
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = std::move(newBinary);
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = newBinarySize;
    }
}

cl_int Program::packDeviceBinary(uint32_t rootDeviceIndex) {
    if (nullptr != buildInfos[rootDeviceIndex].packedDeviceBinary) {
        return CL_SUCCESS;
    }

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto gfxCore = hwInfo->platform.eRenderCoreFamily;
    auto stepping = hwInfo->platform.usRevId;

    if (nullptr != this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()) {
        SingleDeviceBinary singleDeviceBinary;
        singleDeviceBinary.buildOptions = this->options;
        singleDeviceBinary.targetDevice.coreFamily = gfxCore;
        singleDeviceBinary.targetDevice.stepping = stepping;
        singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
        singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize);
        singleDeviceBinary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->debugData.get()), this->debugDataSize);

        std::string packWarnings;
        std::string packErrors;
        auto packedDeviceBinary = NEO::packDeviceBinary(singleDeviceBinary, packErrors, packWarnings);
        if (packedDeviceBinary.empty()) {
            DEBUG_BREAK_IF(true);
            return CL_OUT_OF_HOST_MEMORY;
        }
        this->buildInfos[rootDeviceIndex].packedDeviceBinary = makeCopy(packedDeviceBinary.data(), packedDeviceBinary.size());
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = packedDeviceBinary.size();
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
        this->buildInfos[rootDeviceIndex].packedDeviceBinary = makeCopy(elfData.data(), elfData.size());
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = elfData.size();
    } else {
        return CL_INVALID_PROGRAM;
    }

    return CL_SUCCESS;
}

} // namespace NEO

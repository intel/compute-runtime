/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module_imp.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"

#include "compiler_options.h"
#include "program_debug_data.h"

#include <memory>
#include <unordered_map>

namespace L0 {

namespace BuildOptions {
NEO::ConstStringRef optDisable = "-ze-opt-disable";
NEO::ConstStringRef optLevel = "-ze-opt-level";
NEO::ConstStringRef greaterThan4GbRequired = "-ze-opt-greater-than-4GB-buffer-required";
NEO::ConstStringRef hasBufferOffsetArg = "-ze-intel-has-buffer-offset-arg";
NEO::ConstStringRef debugKernelEnable = "-ze-kernel-debug-enable";
} // namespace BuildOptions

ModuleTranslationUnit::ModuleTranslationUnit(L0::Device *device)
    : device(device) {
}

ModuleTranslationUnit::~ModuleTranslationUnit() {
    if (globalConstBuffer) {
        auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();

        if (svmAllocsManager->getSVMAlloc(reinterpret_cast<void *>(globalConstBuffer->getGpuAddress()))) {
            svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(globalConstBuffer->getGpuAddress()));
        } else {
            this->device->getNEODevice()->getExecutionEnvironment()->memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(globalConstBuffer);
        }
    }

    if (globalVarBuffer) {
        auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();

        if (svmAllocsManager->getSVMAlloc(reinterpret_cast<void *>(globalVarBuffer->getGpuAddress()))) {
            svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(globalVarBuffer->getGpuAddress()));
        } else {
            this->device->getNEODevice()->getExecutionEnvironment()->memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(globalVarBuffer);
        }
    }

    if (this->debugData != nullptr) {
        for (std::vector<char *>::iterator iter = alignedvIsas.begin(); iter != alignedvIsas.end(); ++iter) {
            alignedFree(static_cast<void *>(*iter));
        }
    }
}

std::vector<uint8_t> ModuleTranslationUnit::generateElfFromSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes) {
    NEO::Elf::ElfEncoder<> elfEncoder(true, false, 1U);
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    StackVec<uint32_t, 64> specConstIds;
    StackVec<uint64_t, 64> specConstValues;
    for (uint32_t i = 0; i < static_cast<uint32_t>(inputSpirVs.size()); i++) {
        if (specConstantsValues.size() > 0) {
            specConstIds.clear();
            specConstValues.clear();
            specConstIds.reserve(specConstantsValues.size());
            specConstValues.reserve(specConstantsValues.size());
            for (const auto &specConst : specConstantsValues) {
                specConstIds.push_back(specConst.first);
                specConstValues.push_back(specConst.second);
            }
            elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV_SC_IDS, NEO::Elf::SectionNamesOpenCl::spirvSpecConstIds,
                                     ArrayRef<const uint8_t>::fromAny(specConstIds.begin(), specConstIds.size()));
            elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV_SC_VALUES, NEO::Elf::SectionNamesOpenCl::spirvSpecConstValues,
                                     ArrayRef<const uint8_t>::fromAny(specConstValues.begin(), specConstValues.size()));
        }

        auto sectionType = NEO::Elf::SHT_OPENCL_SPIRV;
        NEO::ConstStringRef sectionName = NEO::Elf::SectionNamesOpenCl::spirvObject;
        elfEncoder.appendSection(sectionType, sectionName, ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(inputSpirVs[i]), inputModuleSizes[i]));
    }

    return elfEncoder.encode();
}

std::string ModuleTranslationUnit::generateCompilerOptions(const char *buildOptions, const char *internalBuildOptions) {
    if (nullptr != buildOptions) {
        options = buildOptions;
    }
    std::string internalOptions = NEO::CompilerOptions::concatenate(internalBuildOptions, BuildOptions::hasBufferOffsetArg);

    if (device->getNEODevice()->getDeviceInfo().debuggerActive) {
        if (NEO::SourceLevelDebugger::shouldAppendOptDisable(*device->getSourceLevelDebugger())) {
            NEO::CompilerOptions::concatenateAppend(options, BuildOptions::optDisable);
        }

        options = NEO::CompilerOptions::concatenate(options, NEO::CompilerOptions::generateDebugInfo);
        internalOptions = NEO::CompilerOptions::concatenate(internalOptions, BuildOptions::debugKernelEnable);
    }

    if (NEO::DebugManager.flags.DisableStatelessToStatefulOptimization.get() ||
        device->getNEODevice()->areSharedSystemAllocationsAllowed()) {
        internalOptions = NEO::CompilerOptions::concatenate(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired);
    }

    return internalOptions;
}

bool ModuleTranslationUnit::processSpecConstantInfo(NEO::CompilerInterface *compilerInterface, const ze_module_constants_t *pConstants, const char *input, uint32_t inputSize) {
    if (pConstants) {
        NEO::SpecConstantInfo specConstInfo;
        auto retVal = compilerInterface->getSpecConstantsInfo(*device->getNEODevice(), ArrayRef<const char>(input, inputSize), specConstInfo);
        if (retVal != NEO::TranslationOutput::ErrorCode::Success) {
            return false;
        }
        for (uint32_t i = 0; i < pConstants->numConstants; i++) {
            uint64_t specConstantValue = 0;
            uint32_t specConstantId = pConstants->pConstantIds[i];
            auto atributeSize = 0u;
            uint32_t j;
            for (j = 0; j < specConstInfo.sizesBuffer->GetSize<uint32_t>(); j++) {
                if (specConstantId == specConstInfo.idsBuffer->GetMemory<uint32_t>()[j]) {
                    atributeSize = specConstInfo.sizesBuffer->GetMemory<uint32_t>()[j];
                    break;
                }
            }
            if (j == specConstInfo.sizesBuffer->GetSize<uint32_t>()) {
                return false;
            }
            memcpy_s(&specConstantValue, sizeof(uint64_t),
                     const_cast<void *>(pConstants->pConstantValues[i]), atributeSize);
            specConstantsValues[specConstantId] = specConstantValue;
        }
    }
    return true;
}

bool ModuleTranslationUnit::compileGenBinary(NEO::TranslationInput inputArgs, bool staticLink) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    UNRECOVERABLE_IF(nullptr == compilerInterface);

    inputArgs.specializedValues = this->specConstantsValues;

    NEO::TranslationOutput compilerOuput = {};
    NEO::TranslationOutput::ErrorCode compilerErr;

    if (staticLink) {
        compilerErr = compilerInterface->link(*device->getNEODevice(), inputArgs, compilerOuput);
    } else {
        compilerErr = compilerInterface->build(*device->getNEODevice(), inputArgs, compilerOuput);
    }

    this->updateBuildLog(compilerOuput.frontendCompilerLog);
    this->updateBuildLog(compilerOuput.backendCompilerLog);

    if (NEO::TranslationOutput::ErrorCode::Success != compilerErr) {
        return false;
    }

    this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
    this->irBinarySize = compilerOuput.intermediateRepresentation.size;
    this->unpackedDeviceBinary = std::move(compilerOuput.deviceBinary.mem);
    this->unpackedDeviceBinarySize = compilerOuput.deviceBinary.size;
    this->debugData = std::move(compilerOuput.debugData.mem);
    this->debugDataSize = compilerOuput.debugData.size;

    return processUnpackedBinary();
}

bool ModuleTranslationUnit::staticLinkSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes, const char *buildOptions, const char *internalBuildOptions,
                                            std::vector<const ze_module_constants_t *> specConstants) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    UNRECOVERABLE_IF(nullptr == compilerInterface);

    std::string internalOptions = this->generateCompilerOptions(buildOptions, internalBuildOptions);

    for (uint32_t i = 0; i < static_cast<uint32_t>(specConstants.size()); i++) {
        auto specConstantResult = this->processSpecConstantInfo(compilerInterface, specConstants[i], inputSpirVs[i], inputModuleSizes[i]);
        if (!specConstantResult) {
            return false;
        }
    }

    NEO::TranslationInput linkInputArgs = {IGC::CodeType::elf, IGC::CodeType::oclGenBin};

    auto spirvElfSource = generateElfFromSpirV(inputSpirVs, inputModuleSizes);

    linkInputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(spirvElfSource.data()), spirvElfSource.size());
    linkInputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
    linkInputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
    return this->compileGenBinary(linkInputArgs, true);
}

bool ModuleTranslationUnit::buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                           const ze_module_constants_t *pConstants) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    UNRECOVERABLE_IF(nullptr == compilerInterface);

    std::string internalOptions = this->generateCompilerOptions(buildOptions, internalBuildOptions);

    auto specConstantResult = this->processSpecConstantInfo(compilerInterface, pConstants, input, inputSize);
    if (!specConstantResult)
        return false;

    NEO::TranslationInput inputArgs = {IGC::CodeType::spirV, IGC::CodeType::oclGenBin};

    inputArgs.src = ArrayRef<const char>(input, inputSize);
    inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
    inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
    return this->compileGenBinary(inputArgs, false);
}

bool ModuleTranslationUnit::createFromNativeBinary(const char *input, size_t inputSize) {
    UNRECOVERABLE_IF((nullptr == device) || (nullptr == device->getNEODevice()));
    auto productAbbreviation = NEO::hardwarePrefix[device->getNEODevice()->getHardwareInfo().platform.eProductFamily];

    NEO::TargetDevice targetDevice = {};
    targetDevice.coreFamily = device->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    targetDevice.productFamily = device->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    targetDevice.stepping = device->getNEODevice()->getHardwareInfo().platform.usRevId;
    targetDevice.maxPointerSizeInBytes = sizeof(uintptr_t);
    std::string decodeErrors;
    std::string decodeWarnings;
    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(input), inputSize);
    auto singleDeviceBinary = unpackSingleDeviceBinary(archive, NEO::ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                       decodeErrors, decodeWarnings);
    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty()) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return false;
    } else {
        this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
        this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
        this->options = singleDeviceBinary.buildOptions.str();

        if (false == singleDeviceBinary.debugData.empty()) {
            this->debugData = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
            this->debugDataSize = singleDeviceBinary.debugData.size();
        }

        bool rebuild = NEO::DebugManager.flags.RebuildPrecompiledKernels.get() && irBinarySize != 0;
        if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == rebuild)) {
            this->unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
            this->unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
            this->packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
            this->packedDeviceBinarySize = archive.size();
        }
    }

    if (nullptr == this->unpackedDeviceBinary) {
        return buildFromSpirV(this->irBinary.get(), static_cast<uint32_t>(this->irBinarySize), this->options.c_str(), "", nullptr);
    } else {
        return processUnpackedBinary();
    }
}

bool ModuleTranslationUnit::processUnpackedBinary() {
    if (0 == unpackedDeviceBinarySize) {
        return false;
    }
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
    NEO::SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    binary.targetDevice.grfSize = device->getHwInfo().capabilityTable.grfSize;
    std::string decodeErrors;
    std::string decodeWarnings;

    NEO::DecodeError decodeError;
    NEO::DeviceBinaryFormat singleDeviceBinaryFormat;
    programInfo.levelZeroDynamicLinkProgram = true;
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings);
    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (NEO::DecodeError::Success != decodeError) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return false;
    }

    processDebugData();

    size_t slmNeeded = NEO::getMaxInlineSlmNeeded(programInfo);
    size_t slmAvailable = 0U;
    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    slmAvailable = static_cast<size_t>(device->getDeviceInfo().localMemSize);
    deviceInfoConstants.maxWorkGroupSize = static_cast<uint32_t>(device->getDeviceInfo().maxWorkGroupSize);
    deviceInfoConstants.computeUnitsUsedForScratch = static_cast<uint32_t>(device->getDeviceInfo().computeUnitsUsedForScratch);
    deviceInfoConstants.slmWindowSize = static_cast<uint32_t>(device->getDeviceInfo().localMemSize);
    if (NEO::requiresLocalMemoryWindowVA(programInfo)) {
        deviceInfoConstants.slmWindow = device->getNEODevice()->getExecutionEnvironment()->memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }

    if (slmNeeded > slmAvailable) {
        return false;
    }

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    if (programInfo.globalConstants.size != 0) {
        this->globalConstBuffer = NEO::allocateGlobalsSurface(svmAllocsManager, *device->getNEODevice(), programInfo.globalConstants.size, true, programInfo.linkerInput.get(), programInfo.globalConstants.initData);
    }

    if (programInfo.globalVariables.size != 0) {
        this->globalVarBuffer = NEO::allocateGlobalsSurface(svmAllocsManager, *device->getNEODevice(), programInfo.globalVariables.size, false, programInfo.linkerInput.get(), programInfo.globalVariables.initData);
    }

    for (auto &kernelInfo : this->programInfo.kernelInfos) {
        kernelInfo->apply(deviceInfoConstants);
    }

    if (programInfo.decodedElf.elfFileHeader) {
        NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

        uint32_t id = 0;
        for (auto &kernelInfo : this->programInfo.kernelInfos) {
            nameToKernelId[kernelInfo->kernelDescriptor.kernelMetadata.kernelName] = id;
            id++;
        }
        programInfo.prepareLinkerInputStorage();
        programInfo.linkerInput->decodeElfSymbolTableAndRelocations(programInfo.decodedElf, nameToKernelId);
    }

    auto gfxCore = device->getNEODevice()->getHardwareInfo().platform.eRenderCoreFamily;
    auto stepping = device->getNEODevice()->getHardwareInfo().platform.usRevId;

    if (this->packedDeviceBinary != nullptr) {
        return true;
    }

    NEO::SingleDeviceBinary singleDeviceBinary;
    singleDeviceBinary.buildOptions = this->options;
    singleDeviceBinary.targetDevice.coreFamily = gfxCore;
    singleDeviceBinary.targetDevice.stepping = stepping;
    singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
    singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize);
    singleDeviceBinary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->debugData.get()), this->debugDataSize);
    std::string packWarnings;
    std::string packErrors;
    auto packedDeviceBinary = NEO::packDeviceBinary(singleDeviceBinary, packErrors, packWarnings);
    if (packedDeviceBinary.empty()) {
        DEBUG_BREAK_IF(true);
        return false;
    }
    this->packedDeviceBinary = makeCopy(packedDeviceBinary.data(), packedDeviceBinary.size());
    this->packedDeviceBinarySize = packedDeviceBinary.size();

    return true;
}

void ModuleTranslationUnit::updateBuildLog(const std::string &newLogEntry) {
    if (newLogEntry.empty() || ('\0' == newLogEntry[0])) {
        return;
    }

    buildLog += newLogEntry.c_str();
    if ('\n' != *buildLog.rbegin()) {
        buildLog.append("\n");
    }
}

void ModuleTranslationUnit::processDebugData() {
    if (this->debugData != nullptr) {
        iOpenCL::SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<iOpenCL::SProgramDebugDataHeaderIGC *>(debugData.get());

        DEBUG_BREAK_IF(programDebugHeader->NumberOfKernels != programInfo.kernelInfos.size());

        const iOpenCL::SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<iOpenCL::SKernelDebugDataHeaderIGC *>(
            ptrOffset(programDebugHeader, sizeof(iOpenCL::SProgramDebugDataHeaderIGC)));

        const char *kernelName = nullptr;
        const char *kernelDebugData = nullptr;

        for (uint32_t i = 0; i < programDebugHeader->NumberOfKernels; i++) {
            kernelName = reinterpret_cast<const char *>(ptrOffset(kernelDebugHeader, sizeof(iOpenCL::SKernelDebugDataHeaderIGC)));

            auto kernelInfo = programInfo.kernelInfos[i];
            UNRECOVERABLE_IF(kernelInfo->kernelDescriptor.kernelMetadata.kernelName.compare(0, kernelInfo->kernelDescriptor.kernelMetadata.kernelName.size(), kernelName) != 0);

            kernelDebugData = ptrOffset(kernelName, kernelDebugHeader->KernelNameSize);

            kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

            char *alignedAlloc = static_cast<char *>(alignedMalloc(kernelDebugHeader->SizeVisaDbgInBytes, MemoryConstants::pageSize));
            memcpy_s(static_cast<void *>(alignedAlloc), kernelDebugHeader->SizeVisaDbgInBytes, kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);

            kernelInfo->kernelDescriptor.external.debugData->vIsa = alignedAlloc;
            kernelInfo->kernelDescriptor.external.debugData->genIsa = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);
            kernelInfo->kernelDescriptor.external.debugData->vIsaSize = kernelDebugHeader->SizeVisaDbgInBytes;
            kernelInfo->kernelDescriptor.external.debugData->genIsaSize = kernelDebugHeader->SizeGenIsaDbgInBytes;

            kernelDebugData = ptrOffset(kernelDebugData, static_cast<size_t>(kernelDebugHeader->SizeVisaDbgInBytes) + kernelDebugHeader->SizeGenIsaDbgInBytes);
            kernelDebugHeader = reinterpret_cast<const iOpenCL::SKernelDebugDataHeaderIGC *>(kernelDebugData);
            alignedvIsas.push_back(alignedAlloc);
        }
    }
}

ModuleImp::ModuleImp(Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type)
    : device(device), translationUnit(std::make_unique<ModuleTranslationUnit>(device)),
      moduleBuildLog(moduleBuildLog), type(type) {
    productFamily = device->getHwInfo().platform.eProductFamily;
}

ModuleImp::~ModuleImp() {
    kernelImmDatas.clear();
}

bool ModuleImp::initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) {
    bool success = true;

    std::string buildOptions;
    std::string internalBuildOptions;

    if (desc->pNext) {
        const ze_base_desc_t *expDesc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
        if (expDesc->stype == ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC) {
            if (desc->format != ZE_MODULE_FORMAT_IL_SPIRV) {
                return false;
            }
            const ze_module_program_exp_desc_t *programExpDesc =
                reinterpret_cast<const ze_module_program_exp_desc_t *>(expDesc);
            std::vector<const char *> inputSpirVs;
            std::vector<uint32_t> inputModuleSizes;
            std::vector<const ze_module_constants_t *> specConstants;

            this->createBuildOptions(nullptr, buildOptions, internalBuildOptions);

            for (uint32_t i = 0; i < static_cast<uint32_t>(programExpDesc->count); i++) {
                std::string tmpBuildOptions;
                std::string tmpInternalBuildOptions;
                inputSpirVs.push_back(reinterpret_cast<const char *>(programExpDesc->pInputModules[i]));
                auto inputSizesInfo = const_cast<size_t *>(programExpDesc->inputSizes);
                uint32_t inputSize = static_cast<uint32_t>(inputSizesInfo[i]);
                inputModuleSizes.push_back(inputSize);
                if (programExpDesc->pConstants) {
                    specConstants.push_back(programExpDesc->pConstants[i]);
                }
                if (programExpDesc->pBuildFlags) {
                    this->createBuildOptions(programExpDesc->pBuildFlags[i], tmpBuildOptions, tmpInternalBuildOptions);
                    buildOptions = buildOptions + tmpBuildOptions;
                    internalBuildOptions = internalBuildOptions + tmpInternalBuildOptions;
                }
            }

            success = this->translationUnit->staticLinkSpirV(inputSpirVs,
                                                             inputModuleSizes,
                                                             buildOptions.c_str(),
                                                             internalBuildOptions.c_str(),
                                                             specConstants);
        } else {
            return false;
        }
    } else {
        this->createBuildOptions(desc->pBuildFlags, buildOptions, internalBuildOptions);

        if (desc->format == ZE_MODULE_FORMAT_NATIVE) {
            success = this->translationUnit->createFromNativeBinary(
                reinterpret_cast<const char *>(desc->pInputModule), desc->inputSize);
        } else if (desc->format == ZE_MODULE_FORMAT_IL_SPIRV) {
            success = this->translationUnit->buildFromSpirV(reinterpret_cast<const char *>(desc->pInputModule),
                                                            static_cast<uint32_t>(desc->inputSize),
                                                            buildOptions.c_str(),
                                                            internalBuildOptions.c_str(),
                                                            desc->pConstants);
        } else {
            return false;
        }
    }

    verifyDebugCapabilities();

    this->updateBuildLog(neoDevice);

    if (false == success) {
        return false;
    }

    kernelImmDatas.reserve(this->translationUnit->programInfo.kernelInfos.size());
    for (auto &ki : this->translationUnit->programInfo.kernelInfos) {
        std::unique_ptr<KernelImmutableData> kernelImmData{new KernelImmutableData(this->device)};
        kernelImmData->initialize(ki, device, device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  this->translationUnit->globalConstBuffer, this->translationUnit->globalVarBuffer,
                                  this->type == ModuleType::Builtin);
        kernelImmDatas.push_back(std::move(kernelImmData));
    }
    this->maxGroupSize = static_cast<uint32_t>(this->translationUnit->device->getNEODevice()->getDeviceInfo().maxWorkGroupSize);

    checkIfPrivateMemoryPerDispatchIsNeeded();

    if (debugEnabled) {
        if (device->getSourceLevelDebugger()) {
            for (auto kernelInfo : this->translationUnit->programInfo.kernelInfos) {
                NEO::DebugData *notifyDebugData = kernelInfo->kernelDescriptor.external.debugData.get();
                NEO::DebugData relocatedDebugData;

                if (kernelInfo->kernelDescriptor.external.relocatedDebugData.get()) {
                    relocatedDebugData.genIsa = kernelInfo->kernelDescriptor.external.debugData->genIsa;
                    relocatedDebugData.genIsaSize = kernelInfo->kernelDescriptor.external.debugData->genIsaSize;
                    relocatedDebugData.vIsa = reinterpret_cast<char *>(kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
                    relocatedDebugData.vIsaSize = kernelInfo->kernelDescriptor.external.debugData->vIsaSize;
                    notifyDebugData = &relocatedDebugData;
                }

                device->getSourceLevelDebugger()->notifyKernelDebugData(notifyDebugData,
                                                                        kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
                                                                        kernelInfo->heapInfo.pKernelHeap,
                                                                        kernelInfo->heapInfo.KernelHeapSize);
            }
        }
    }

    return this->linkBinary();
}

const KernelImmutableData *ModuleImp::getKernelImmutableData(const char *functionName) const {
    for (auto &kernelImmData : kernelImmDatas) {
        if (kernelImmData->getDescriptor().kernelMetadata.kernelName.compare(functionName) == 0) {
            return kernelImmData.get();
        }
    }
    return nullptr;
}

void ModuleImp::createBuildOptions(const char *pBuildFlags, std::string &apiOptions, std::string &internalBuildOptions) {
    if (pBuildFlags != nullptr) {
        std::string buildFlags(pBuildFlags);

        apiOptions = pBuildFlags;
        moveBuildOption(apiOptions, apiOptions, NEO::CompilerOptions::optDisable, BuildOptions::optDisable);
        moveBuildOption(apiOptions, apiOptions, NEO::CompilerOptions::optLevel, BuildOptions::optLevel);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired, BuildOptions::greaterThan4GbRequired);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::allowZebin, NEO::CompilerOptions::allowZebin);

        createBuildExtraOptions(apiOptions, internalBuildOptions);
    }
    if (NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        NEO::CompilerOptions::concatenateAppend(internalBuildOptions, NEO::CompilerOptions::bindlessMode.str());
    }
}

void ModuleImp::updateBuildLog(NEO::Device *neoDevice) {
    if (this->moduleBuildLog) {
        moduleBuildLog->appendString(this->translationUnit->buildLog.c_str(), this->translationUnit->buildLog.size());
    }
}

ze_result_t ModuleImp::createKernel(const ze_kernel_desc_t *desc,
                                    ze_kernel_handle_t *phFunction) {
    ze_result_t res;
    if (!isFullyLinked) {
        return ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED;
    }
    auto kernel = Kernel::create(productFamily, this, desc, &res);

    if (res == ZE_RESULT_SUCCESS) {
        *phFunction = kernel->toHandle();
    }

    return res;
}

ze_result_t ModuleImp::getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) {
    auto genBinary = this->translationUnit->packedDeviceBinary.get();

    *pSize = this->translationUnit->packedDeviceBinarySize;
    if (pModuleNativeBinary != nullptr) {
        memcpy_s(pModuleNativeBinary, this->translationUnit->packedDeviceBinarySize, genBinary, this->translationUnit->packedDeviceBinarySize);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) {
    if (translationUnit == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    if (pDebugData == nullptr) {
        *pDebugDataSize = translationUnit->debugDataSize;
        return ZE_RESULT_SUCCESS;
    }
    memcpy_s(pDebugData, *pDebugDataSize, translationUnit->debugData.get(), translationUnit->debugDataSize);
    return ZE_RESULT_SUCCESS;
}

void ModuleImp::copyPatchedSegments(const NEO::Linker::PatchableSegments &isaSegmentsForPatching) {
    if (this->translationUnit->programInfo.linkerInput && this->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        for (const auto &kernelImmData : this->kernelImmDatas) {
            if (nullptr == kernelImmData->getIsaGraphicsAllocation()) {
                continue;
            }

            kernelImmData->getIsaGraphicsAllocation()->setTbxWritable(true, std::numeric_limits<uint32_t>::max());
            kernelImmData->getIsaGraphicsAllocation()->setAubWritable(true, std::numeric_limits<uint32_t>::max());
            auto segmentId = &kernelImmData - &this->kernelImmDatas[0];
            this->device->getDriverHandle()->getMemoryManager()->copyMemoryToAllocation(kernelImmData->getIsaGraphicsAllocation(), 0,
                                                                                        isaSegmentsForPatching[segmentId].hostPointer,
                                                                                        isaSegmentsForPatching[segmentId].segmentSize);
        }
    }
}

bool ModuleImp::linkBinary() {
    using namespace NEO;
    auto linkerInput = this->translationUnit->programInfo.linkerInput.get();
    if (linkerInput == nullptr) {
        isFullyLinked = true;
        return true;
    }
    Linker linker(*linkerInput);
    Linker::SegmentInfo globals;
    Linker::SegmentInfo constants;
    Linker::SegmentInfo exportedFunctions;
    GraphicsAllocation *globalsForPatching = translationUnit->globalVarBuffer;
    GraphicsAllocation *constantsForPatching = translationUnit->globalConstBuffer;
    if (globalsForPatching != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(globalsForPatching->getGpuAddress());
        globals.segmentSize = globalsForPatching->getUnderlyingBufferSize();
    }
    if (constantsForPatching != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(constantsForPatching->getGpuAddress());
        constants.segmentSize = constantsForPatching->getUnderlyingBufferSize();
    }
    if (linkerInput->getExportedFunctionsSegmentId() >= 0) {
        auto exportedFunctionHeapId = linkerInput->getExportedFunctionsSegmentId();
        this->exportedFunctionsSurface = this->kernelImmDatas[exportedFunctionHeapId]->getIsaGraphicsAllocation();
        exportedFunctions.gpuAddress = static_cast<uintptr_t>(exportedFunctionsSurface->getGpuAddressToPatch());
        exportedFunctions.segmentSize = exportedFunctionsSurface->getUnderlyingBufferSize();
    }
    Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
    if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        patchedIsaTempStorage.reserve(this->kernelImmDatas.size());
        for (const auto &kernelInfo : this->translationUnit->programInfo.kernelInfos) {
            auto &kernHeapInfo = kernelInfo->heapInfo;
            const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.KernelHeapSize));
            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), kernHeapInfo.KernelHeapSize});
        }
    }

    auto linkStatus = linker.link(globals, constants, exportedFunctions,
                                  globalsForPatching, constantsForPatching,
                                  isaSegmentsForPatching, unresolvedExternalsInfo, this->device->getNEODevice(),
                                  translationUnit->programInfo.globalConstants.initData,
                                  translationUnit->programInfo.globalVariables.initData);
    this->symbols = linker.extractRelocatedSymbols();
    if (LinkingStatus::LinkedFully != linkStatus) {
        if (moduleBuildLog) {
            std::vector<std::string> kernelNames;
            for (const auto &kernelInfo : this->translationUnit->programInfo.kernelInfos) {
                kernelNames.push_back("kernel : " + kernelInfo->kernelDescriptor.kernelMetadata.kernelName);
            }
            auto error = constructLinkerErrorMessage(unresolvedExternalsInfo, kernelNames);
            moduleBuildLog->appendString(error.c_str(), error.size());
        }
        isFullyLinked = false;
        return LinkingStatus::LinkedPartially == linkStatus;
    } else {
        copyPatchedSegments(isaSegmentsForPatching);
    }
    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->symbols));
    isFullyLinked = true;
    for (auto kernelId = 0u; kernelId < kernelImmDatas.size(); kernelId++) {
        auto &kernImmData = kernelImmDatas[kernelId];

        kernImmData->getResidencyContainer().reserve(kernImmData->getResidencyContainer().size() +
                                                     ((this->exportedFunctionsSurface != nullptr) ? 1 : 0) + this->importedSymbolAllocations.size());

        if (nullptr != this->exportedFunctionsSurface) {
            kernImmData->getResidencyContainer().push_back(this->exportedFunctionsSurface);
        }
        kernImmData->getResidencyContainer().insert(kernImmData->getResidencyContainer().end(), this->importedSymbolAllocations.begin(),
                                                    this->importedSymbolAllocations.end());

        auto &kernelDescriptor = const_cast<KernelDescriptor &>(kernImmData->getDescriptor());
        kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = linkerInput->areImplicitArgsRequired(kernelId);
    }
    return true;
}

ze_result_t ModuleImp::getFunctionPointer(const char *pFunctionName, void **pfnFunction) {
    auto symbolIt = symbols.find(pFunctionName);
    if ((symbolIt == symbols.end()) || (symbolIt->second.symbol.segment != NEO::SegmentType::Instructions)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *pfnFunction = reinterpret_cast<void *>(symbolIt->second.gpuAddress);
    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::getGlobalPointer(const char *pGlobalName, size_t *pSize, void **pPtr) {
    auto symbolIt = symbols.find(pGlobalName);
    if ((symbolIt == symbols.end()) || (symbolIt->second.symbol.segment == NEO::SegmentType::Instructions)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (pPtr) {
        *pPtr = reinterpret_cast<void *>(symbolIt->second.gpuAddress);
    }
    if (pSize) {
        *pSize = symbolIt->second.symbol.size;
    }
    return ZE_RESULT_SUCCESS;
}

Module *Module::create(Device *device, const ze_module_desc_t *desc,
                       ModuleBuildLog *moduleBuildLog, ModuleType type) {
    auto module = new ModuleImp(device, moduleBuildLog, type);

    bool success = module->initialize(desc, device->getNEODevice());
    if (success == false) {
        module->destroy();
        return nullptr;
    }

    return module;
}

ze_result_t ModuleImp::getKernelNames(uint32_t *pCount, const char **pNames) {
    auto &kernelImmDatas = this->getKernelImmutableDataVector();
    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(kernelImmDatas.size());
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > static_cast<uint32_t>(kernelImmDatas.size())) {
        *pCount = static_cast<uint32_t>(kernelImmDatas.size());
    }

    uint32_t outCount = 0;
    for (auto &kernelImmData : kernelImmDatas) {
        *(pNames + outCount) = kernelImmData->getDescriptor().kernelMetadata.kernelName.c_str();
        outCount++;
        if (outCount == *pCount) {
            break;
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool ModuleImp::isDebugEnabled() const {
    return debugEnabled;
}

void ModuleImp::verifyDebugCapabilities() {
    bool debugCapabilities = device->getNEODevice()->getDebugger() != nullptr;

    if (debugCapabilities) {
        //verify all kernels are debuggable
        for (auto kernelInfo : this->translationUnit->programInfo.kernelInfos) {
            bool systemThreadSurfaceAvailable = NEO::isValidOffset(kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful) ||
                                                NEO::isValidOffset(kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindless);

            debugCapabilities &= systemThreadSurfaceAvailable;
        }
    }
    debugEnabled = debugCapabilities;
}

void ModuleImp::checkIfPrivateMemoryPerDispatchIsNeeded() {
    size_t modulePrivateMemorySize = 0;
    for (auto &kernelImmData : this->kernelImmDatas) {
        if (0 == kernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize) {
            continue;
        }
        auto kernelPrivateMemorySize = NEO::KernelHelper::getPrivateSurfaceSize(kernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize,
                                                                                this->device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch);
        modulePrivateMemorySize += kernelPrivateMemorySize;
    }

    this->allocatePrivateMemoryPerDispatch = false;
    if (modulePrivateMemorySize > 0U) {
        auto globalMemorySize = device->getNEODevice()->getRootDevice()->getGlobalMemorySize(static_cast<uint32_t>(device->getNEODevice()->getDeviceBitfield().to_ulong()));
        this->allocatePrivateMemoryPerDispatch = modulePrivateMemorySize > globalMemorySize;
    }
}

ze_result_t ModuleImp::getProperties(ze_module_properties_t *pModuleProperties) {

    pModuleProperties->flags = 0;

    if (!unresolvedExternalsInfo.empty()) {
        pModuleProperties->flags |= ZE_MODULE_PROPERTY_FLAG_IMPORTS;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::performDynamicLink(uint32_t numModules,
                                          ze_module_handle_t *phModules,
                                          ze_module_build_log_handle_t *phLinkLog) {
    ModuleBuildLog *moduleLinkLog = nullptr;
    if (phLinkLog) {
        moduleLinkLog = ModuleBuildLog::create();
        *phLinkLog = moduleLinkLog->toHandle();
    }
    for (auto i = 0u; i < numModules; i++) {
        auto moduleId = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
        if (moduleId->isFullyLinked) {
            continue;
        }
        NEO::Linker::PatchableSegments isaSegmentsForPatching;
        std::vector<std::vector<char>> patchedIsaTempStorage;
        uint32_t numPatchedSymbols = 0u;
        std::vector<std::string> unresolvedSymbolLogMessages;
        if (moduleId->translationUnit->programInfo.linkerInput && moduleId->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
            patchedIsaTempStorage.reserve(moduleId->kernelImmDatas.size());
            for (const auto &kernelInfo : moduleId->translationUnit->programInfo.kernelInfos) {
                auto &kernHeapInfo = kernelInfo->heapInfo;
                const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
                patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.KernelHeapSize));
                isaSegmentsForPatching.push_back(NEO::Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), kernHeapInfo.KernelHeapSize});
            }
            for (const auto &unresolvedExternal : moduleId->unresolvedExternalsInfo) {
                if (moduleLinkLog) {
                    std::stringstream logMessage;
                    logMessage << "Module <" << moduleId << ">: "
                               << " Unresolved Symbol <" << unresolvedExternal.unresolvedRelocation.symbolName << ">";
                    unresolvedSymbolLogMessages.push_back(logMessage.str());
                }
                for (auto i = 0u; i < numModules; i++) {
                    auto moduleHandle = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
                    auto symbolIt = moduleHandle->symbols.find(unresolvedExternal.unresolvedRelocation.symbolName);
                    if (symbolIt != moduleHandle->symbols.end()) {
                        auto relocAddress = ptrOffset(isaSegmentsForPatching[unresolvedExternal.instructionsSegmentId].hostPointer,
                                                      static_cast<uintptr_t>(unresolvedExternal.unresolvedRelocation.offset));

                        NEO::Linker::patchAddress(relocAddress, symbolIt->second, unresolvedExternal.unresolvedRelocation);
                        numPatchedSymbols++;
                        moduleId->importedSymbolAllocations.insert(moduleHandle->exportedFunctionsSurface);

                        if (moduleLinkLog) {
                            std::stringstream logMessage;
                            logMessage << " Successfully Resolved Thru Dynamic Link to Module <" << moduleHandle << ">";
                            unresolvedSymbolLogMessages.back().append(logMessage.str());
                        }

                        // Apply the exported functions surface state from the export module to the import module if it exists.
                        // Enables import modules to access the exported functions during kernel execution.
                        for (auto &kernImmData : moduleId->kernelImmDatas) {
                            kernImmData->getResidencyContainer().reserve(kernImmData->getResidencyContainer().size() +
                                                                         ((moduleHandle->exportedFunctionsSurface != nullptr) ? 1 : 0) + moduleId->importedSymbolAllocations.size());

                            if (nullptr != moduleHandle->exportedFunctionsSurface) {
                                kernImmData->getResidencyContainer().push_back(moduleHandle->exportedFunctionsSurface);
                            }
                            kernImmData->getResidencyContainer().insert(kernImmData->getResidencyContainer().end(), moduleId->importedSymbolAllocations.begin(),
                                                                        moduleId->importedSymbolAllocations.end());
                        }
                        break;
                    }
                }
            }
        }
        if (moduleLinkLog) {
            for (int i = 0; i < (int)unresolvedSymbolLogMessages.size(); i++) {
                moduleLinkLog->appendString(unresolvedSymbolLogMessages[i].c_str(), unresolvedSymbolLogMessages[i].size());
            }
        }
        if (numPatchedSymbols != moduleId->unresolvedExternalsInfo.size()) {
            return ZE_RESULT_ERROR_MODULE_LINK_FAILURE;
        }
        moduleId->copyPatchedSegments(isaSegmentsForPatching);
        moduleId->isFullyLinked = true;
    }
    return ZE_RESULT_SUCCESS;
}

bool moveBuildOption(std::string &dstOptionsSet, std::string &srcOptionSet, NEO::ConstStringRef dstOptionName, NEO::ConstStringRef srcOptionName) {
    const char optDelim = ' ';
    const char valDelim = '=';

    auto optInSrcPos = srcOptionSet.find(srcOptionName.begin());
    if (std::string::npos == optInSrcPos) {
        return false;
    }

    std::string dstOptionStr(dstOptionName);
    auto optInSrcEndPos = srcOptionSet.find(optDelim, optInSrcPos);
    if (srcOptionName == BuildOptions::optLevel) {
        auto valInSrcPos = srcOptionSet.find(valDelim, optInSrcPos);
        if (std::string::npos == valInSrcPos) {
            return false;
        }
        dstOptionStr += srcOptionSet.substr(valInSrcPos + 1, optInSrcEndPos);
    }
    srcOptionSet.erase(optInSrcPos, (optInSrcEndPos - optInSrcPos));
    NEO::CompilerOptions::concatenateAppend(dstOptionsSet, dstOptionStr);
    return true;
}

} // namespace L0

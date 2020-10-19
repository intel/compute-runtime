/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module_imp.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/program/kernel_info.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"

#include "compiler_options.h"
#include "program_debug_data.h"

#include <memory>

namespace L0 {

namespace BuildOptions {
NEO::ConstStringRef optDisable = "-ze-opt-disable";
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
}

bool ModuleTranslationUnit::buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                           const ze_module_constants_t *pConstants) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    UNRECOVERABLE_IF(nullptr == compilerInterface);
    UNRECOVERABLE_IF((nullptr == device) || (nullptr == device->getNEODevice()));

    if (nullptr != buildOptions) {
        options = buildOptions;
    }
    std::string internalOptions = NEO::CompilerOptions::concatenate(internalBuildOptions, BuildOptions::hasBufferOffsetArg);

    if (device->getNEODevice()->getDeviceInfo().debuggerActive) {
        if (device->getSourceLevelDebugger()->isOptimizationDisabled()) {
            NEO::CompilerOptions::concatenateAppend(options, BuildOptions::optDisable);
        }
        options = NEO::CompilerOptions::concatenate(options, NEO::CompilerOptions::generateDebugInfo);
        internalOptions = NEO::CompilerOptions::concatenate(internalOptions, BuildOptions::debugKernelEnable);
    }

    NEO::TranslationInput inputArgs = {IGC::CodeType::spirV, IGC::CodeType::oclGenBin};

    if (pConstants) {
        for (uint32_t i = 0; i < pConstants->numConstants; i++) {
            uint64_t specConstantValue = 0;
            memcpy_s(&specConstantValue, sizeof(uint64_t),
                     const_cast<void *>(pConstants->pConstantValues[i]), sizeof(uint64_t));
            uint32_t specConstantId = pConstants->pConstantIds[i];
            specConstantsValues[specConstantId] = specConstantValue;
        }
    }

    inputArgs.src = ArrayRef<const char>(input, inputSize);
    inputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
    inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
    inputArgs.specializedValues = this->specConstantsValues;
    NEO::TranslationOutput compilerOuput = {};
    auto compilerErr = compilerInterface->build(*device->getNEODevice(), inputArgs, compilerOuput);
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
    std::string decodeErrors;
    std::string decodeWarnings;

    NEO::DecodeError decodeError;
    NEO::DeviceBinaryFormat singleDeviceBinaryFormat;
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

            kernelInfo->kernelDescriptor.external.debugData->vIsa = kernelDebugData;
            kernelInfo->kernelDescriptor.external.debugData->genIsa = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);
            kernelInfo->kernelDescriptor.external.debugData->vIsaSize = kernelDebugHeader->SizeVisaDbgInBytes;
            kernelInfo->kernelDescriptor.external.debugData->genIsaSize = kernelDebugHeader->SizeGenIsaDbgInBytes;

            kernelDebugData = ptrOffset(kernelDebugData, static_cast<size_t>(kernelDebugHeader->SizeVisaDbgInBytes) + kernelDebugHeader->SizeGenIsaDbgInBytes);
            kernelDebugHeader = reinterpret_cast<const iOpenCL::SKernelDebugDataHeaderIGC *>(kernelDebugData);
        }
    }
}

ModuleImp::ModuleImp(Device *device, ModuleBuildLog *moduleBuildLog)
    : device(device), translationUnit(std::make_unique<ModuleTranslationUnit>(device)),
      moduleBuildLog(moduleBuildLog) {
    productFamily = device->getHwInfo().platform.eProductFamily;
}

ModuleImp::~ModuleImp() {
    kernelImmDatas.clear();
}

bool ModuleImp::initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) {
    bool success = true;
    NEO::useKernelDescriptor = true;

    std::string buildOptions;
    std::string internalBuildOptions;

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

    verifyDebugCapabilities();

    this->updateBuildLog(neoDevice);

    if (debugEnabled) {
        for (auto kernelInfo : this->translationUnit->programInfo.kernelInfos) {
            device->getSourceLevelDebugger()->notifyKernelDebugData(kernelInfo->kernelDescriptor.external.debugData.get(),
                                                                    kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
                                                                    kernelInfo->heapInfo.pKernelHeap,
                                                                    kernelInfo->heapInfo.KernelHeapSize);
        }
    }

    if (false == success) {
        return false;
    }

    kernelImmDatas.reserve(this->translationUnit->programInfo.kernelInfos.size());
    for (auto &ki : this->translationUnit->programInfo.kernelInfos) {
        std::unique_ptr<KernelImmutableData> kernelImmData{new KernelImmutableData(this->device)};
        kernelImmData->initialize(ki, *(device->getNEODevice()->getMemoryManager()),
                                  device->getNEODevice(),
                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                  this->translationUnit->globalConstBuffer, this->translationUnit->globalVarBuffer);
        kernelImmDatas.push_back(std::move(kernelImmData));
    }
    this->maxGroupSize = static_cast<uint32_t>(this->translationUnit->device->getNEODevice()->getDeviceInfo().maxWorkGroupSize);

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
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired, BuildOptions::greaterThan4GbRequired);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::allowZebin, NEO::CompilerOptions::allowZebin);
        createBuildExtraOptions(apiOptions, internalBuildOptions);
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
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
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
            auto segmentId = &kernelImmData - &this->kernelImmDatas[0];
            this->device->getDriverHandle()->getMemoryManager()->copyMemoryToAllocation(kernelImmData->getIsaGraphicsAllocation(), 0,
                                                                                        isaSegmentsForPatching[segmentId].hostPointer,
                                                                                        isaSegmentsForPatching[segmentId].segmentSize);
        }
    }
}

bool ModuleImp::linkBinary() {
    using namespace NEO;
    if (this->translationUnit->programInfo.linkerInput == nullptr) {
        isFullyLinked = true;
        return true;
    }
    Linker linker(*this->translationUnit->programInfo.linkerInput);
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
    if (this->translationUnit->programInfo.linkerInput->getExportedFunctionsSegmentId() >= 0) {
        auto exportedFunctionHeapId = this->translationUnit->programInfo.linkerInput->getExportedFunctionsSegmentId();
        this->exportedFunctionsSurface = this->kernelImmDatas[exportedFunctionHeapId]->getIsaGraphicsAllocation();
        exportedFunctions.gpuAddress = static_cast<uintptr_t>(exportedFunctionsSurface->getGpuAddressToPatch());
        exportedFunctions.segmentSize = exportedFunctionsSurface->getUnderlyingBufferSize();
    }
    Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
    if (this->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
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
        return LinkingStatus::LinkedPartially == linkStatus;
    } else {
        copyPatchedSegments(isaSegmentsForPatching);
    }
    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->symbols));
    isFullyLinked = true;
    for (auto &kernImmData : this->kernelImmDatas) {
        kernImmData->getResidencyContainer().reserve(kernImmData->getResidencyContainer().size() +
                                                     ((this->exportedFunctionsSurface != nullptr) ? 1 : 0) + this->importedSymbolAllocations.size());

        if (nullptr != this->exportedFunctionsSurface) {
            kernImmData->getResidencyContainer().push_back(this->exportedFunctionsSurface);
        }
        kernImmData->getResidencyContainer().insert(kernImmData->getResidencyContainer().end(), this->importedSymbolAllocations.begin(),
                                                    this->importedSymbolAllocations.end());
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

ze_result_t ModuleImp::getGlobalPointer(const char *pGlobalName, void **pPtr) {
    auto symbolIt = symbols.find(pGlobalName);
    if ((symbolIt == symbols.end()) || (symbolIt->second.symbol.segment == NEO::SegmentType::Instructions)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *pPtr = reinterpret_cast<void *>(symbolIt->second.gpuAddress);
    return ZE_RESULT_SUCCESS;
}

Module *Module::create(Device *device, const ze_module_desc_t *desc,
                       ModuleBuildLog *moduleBuildLog) {
    auto module = new ModuleImp(device, moduleBuildLog);

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
    bool debugCapabilities = device->getNEODevice()->isDebuggerActive();

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

ze_result_t ModuleImp::performDynamicLink(uint32_t numModules,
                                          ze_module_handle_t *phModules,
                                          ze_module_build_log_handle_t *phLinkLog) {

    for (auto i = 0u; i < numModules; i++) {
        auto moduleId = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
        if (moduleId->isFullyLinked) {
            continue;
        }
        NEO::Linker::PatchableSegments isaSegmentsForPatching;
        std::vector<std::vector<char>> patchedIsaTempStorage;
        uint32_t numPatchedSymbols = 0u;
        if (moduleId->translationUnit->programInfo.linkerInput && moduleId->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
            patchedIsaTempStorage.reserve(moduleId->kernelImmDatas.size());
            for (const auto &kernelInfo : moduleId->translationUnit->programInfo.kernelInfos) {
                auto &kernHeapInfo = kernelInfo->heapInfo;
                const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
                patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.KernelHeapSize));
                isaSegmentsForPatching.push_back(NEO::Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), kernHeapInfo.KernelHeapSize});
            }
            for (const auto &unresolvedExternal : moduleId->unresolvedExternalsInfo) {
                for (auto i = 0u; i < numModules; i++) {
                    auto moduleHandle = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));

                    auto symbolIt = moduleHandle->symbols.find(unresolvedExternal.unresolvedRelocation.symbolName);
                    if (symbolIt != moduleHandle->symbols.end()) {
                        auto relocAddress = ptrOffset(isaSegmentsForPatching[unresolvedExternal.instructionsSegmentId].hostPointer,
                                                      static_cast<uintptr_t>(unresolvedExternal.unresolvedRelocation.offset));

                        NEO::Linker::patchAddress(relocAddress, symbolIt->second, unresolvedExternal.unresolvedRelocation);
                        numPatchedSymbols++;
                        moduleId->importedSymbolAllocations.insert(moduleHandle->exportedFunctionsSurface);
                        break;
                    }
                }
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
    auto optInSrcPos = srcOptionSet.find(srcOptionName.begin());
    if (std::string::npos == optInSrcPos) {
        return false;
    }

    srcOptionSet.erase(optInSrcPos, srcOptionName.length());
    NEO::CompilerOptions::concatenateAppend(dstOptionsSet, dstOptionName);
    return true;
}

} // namespace L0

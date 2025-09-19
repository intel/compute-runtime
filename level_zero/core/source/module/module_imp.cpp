/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_options_extra.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/device_binary_format/zebin/debug_zebin.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/metadata_generation.h"
#include "shared/source/program/program_initialization.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module_build_log.h"

#include "program_debug_data.h"

#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>
namespace L0 {

namespace BuildOptions {
NEO::ConstStringRef optDisable = "-ze-opt-disable";
NEO::ConstStringRef optLevel = "-ze-opt-level";
NEO::ConstStringRef greaterThan4GbRequired = "-ze-opt-greater-than-4GB-buffer-required";
NEO::ConstStringRef hasBufferOffsetArg = "-ze-intel-has-buffer-offset-arg";
NEO::ConstStringRef debugKernelEnable = "-ze-kernel-debug-enable";
NEO::ConstStringRef profileFlags = "-zet-profile-flags";
NEO::ConstStringRef optLargeRegisterFile = "-ze-opt-large-register-file";
NEO::ConstStringRef optAutoGrf = "-ze-intel-enable-auto-large-GRF-mode";
NEO::ConstStringRef enableLibraryCompile = "-library-compilation";
NEO::ConstStringRef enableGlobalVariableSymbols = "-ze-take-global-address";
NEO::ConstStringRef enableFP64GenEmu = "-ze-fp64-gen-emu";
} // namespace BuildOptions

ModuleTranslationUnit::ModuleTranslationUnit(L0::Device *device)
    : device(device) {
}

ModuleTranslationUnit::~ModuleTranslationUnit() {
    freeGlobalBufferAllocation(globalConstBuffer);
    freeGlobalBufferAllocation(globalVarBuffer);

    if (this->debugData != nullptr) {
        for (std::vector<char *>::iterator iter = alignedvIsas.begin(); iter != alignedvIsas.end(); ++iter) {
            alignedFree(static_cast<void *>(*iter));
        }
    }
}

void ModuleTranslationUnit::freeGlobalBufferAllocation(const std::unique_ptr<NEO::SharedPoolAllocation> &globalBuffer) {
    if (!globalBuffer) {
        return;
    }

    auto graphicsAllocation = globalBuffer->getGraphicsAllocation();
    if (!graphicsAllocation) {
        return;
    }

    auto gpuAddress = reinterpret_cast<void *>(globalBuffer->getGpuAddress());

    if (auto usmPool = device->getNEODevice()->getUsmConstantSurfaceAllocPool();
        usmPool && usmPool->isInPool(gpuAddress)) {
        [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
        DEBUG_BREAK_IF(!ret);
        return;
    }

    if (auto usmPool = device->getNEODevice()->getUsmGlobalSurfaceAllocPool();
        usmPool && usmPool->isInPool(gpuAddress)) {
        [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
        DEBUG_BREAK_IF(!ret);
        return;
    }

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();

    if (svmAllocsManager->getSVMAlloc(gpuAddress)) {
        svmAllocsManager->freeSVMAlloc(gpuAddress);
    } else {
        this->device->getNEODevice()->getExecutionEnvironment()->memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
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
    auto &neoDevice = *device->getNEODevice();

    if (neoDevice.getExecutionEnvironment()->isFP64EmulationEnabled()) {
        internalOptions = NEO::CompilerOptions::concatenate(internalOptions, BuildOptions::enableFP64GenEmu);
    }

    const auto &compilerProductHelper = neoDevice.getRootDeviceEnvironment().getHelper<NEO::CompilerProductHelper>();
    auto forceToStatelessRequired = compilerProductHelper.isForceToStatelessRequired();
    auto statelessToStatefulOptimizationDisabled = NEO::debugManager.flags.DisableStatelessToStatefulOptimization.get();

    if (forceToStatelessRequired || statelessToStatefulOptimizationDisabled) {
        internalOptions = NEO::CompilerOptions::concatenate(internalOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired);
    }
    bool isDebuggerActive = neoDevice.getDebugger() != nullptr;
    NEO::CompilerOptions::concatenateAppend(internalOptions, compilerProductHelper.getCachingPolicyOptions(isDebuggerActive));

    NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, device->getHwInfo(), compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);
    return internalOptions;
}

bool ModuleTranslationUnit::processSpecConstantInfo(NEO::CompilerInterface *compilerInterface, const ze_module_constants_t *pConstants, const char *input, uint32_t inputSize) {
    if (pConstants) {
        NEO::SpecConstantInfo specConstInfo;
        auto retVal = compilerInterface->getSpecConstantsInfo(*device->getNEODevice(), ArrayRef<const char>(input, inputSize), specConstInfo);
        if (retVal != NEO::TranslationOutput::ErrorCode::success) {
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

ze_result_t ModuleTranslationUnit::compileGenBinary(NEO::TranslationInput &inputArgs, bool staticLink) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (!compilerInterface) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

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

    if (NEO::TranslationOutput::ErrorCode::success != compilerErr) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    this->irBinary = std::move(compilerOuput.intermediateRepresentation.mem);
    this->irBinarySize = compilerOuput.intermediateRepresentation.size;
    this->unpackedDeviceBinary = std::move(compilerOuput.deviceBinary.mem);
    this->unpackedDeviceBinarySize = compilerOuput.deviceBinary.size;
    this->debugData = std::move(compilerOuput.debugData.mem);
    this->debugDataSize = compilerOuput.debugData.size;

    return processUnpackedBinary();
}

ze_result_t ModuleTranslationUnit::staticLinkSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes, const char *buildOptions, const char *internalBuildOptions,
                                                   std::vector<const ze_module_constants_t *> specConstants) {
    auto compilerInterface = device->getNEODevice()->getCompilerInterface();
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (!compilerInterface) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    std::string internalOptions = this->generateCompilerOptions(buildOptions, internalBuildOptions);

    for (uint32_t i = 0; i < static_cast<uint32_t>(specConstants.size()); i++) {
        auto specConstantResult = this->processSpecConstantInfo(compilerInterface, specConstants[i], inputSpirVs[i], inputModuleSizes[i]);
        if (!specConstantResult) {
            driverHandle->clearErrorDescription();
            return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        }
    }

    NEO::TranslationInput linkInputArgs = {IGC::CodeType::elf, IGC::CodeType::oclGenBin};

    auto spirvElfSource = generateElfFromSpirV(inputSpirVs, inputModuleSizes);

    linkInputArgs.src = ArrayRef<const char>(reinterpret_cast<const char *>(spirvElfSource.data()), spirvElfSource.size());
    linkInputArgs.apiOptions = ArrayRef<const char>(options.c_str(), options.length());
    linkInputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
    return this->compileGenBinary(linkInputArgs, true);
}

ze_result_t ModuleTranslationUnit::buildFromIntermediate(IGC::CodeType::CodeType_t intermediateType, const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                                         const ze_module_constants_t *pConstants) {
    const auto &neoDevice = device->getNEODevice();
    auto compilerInterface = neoDevice->getCompilerInterface();
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (!compilerInterface) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    auto specConstantResult = this->processSpecConstantInfo(compilerInterface, pConstants, input, inputSize);
    if (!specConstantResult) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    std::string internalOptions = this->generateCompilerOptions(buildOptions, internalBuildOptions);

    NEO::TranslationInput inputArgs = {intermediateType, IGC::CodeType::oclGenBin};

    inputArgs.src = ArrayRef<const char>(input, inputSize);
    inputArgs.apiOptions = ArrayRef<const char>(this->options.c_str(), this->options.length());
    inputArgs.internalOptions = ArrayRef<const char>(internalOptions.c_str(), internalOptions.length());
    return this->compileGenBinary(inputArgs, false);
}

ze_result_t ModuleTranslationUnit::createFromNativeBinary(const char *input, size_t inputSize, const char *internalBuildOptions) {
    UNRECOVERABLE_IF((nullptr == device) || (nullptr == device->getNEODevice()));
    auto productAbbreviation = NEO::hardwarePrefix[device->getNEODevice()->getHardwareInfo().platform.eProductFamily];

    NEO::TargetDevice targetDevice = NEO::getTargetDevice(device->getNEODevice()->getRootDeviceEnvironment());
    std::string decodeErrors;
    std::string decodeWarnings;
    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(input), inputSize);
    auto singleDeviceBinary = unpackSingleDeviceBinary(archive, NEO::ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                       decodeErrors, decodeWarnings);
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }
    if (singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty()) {
        CREATE_DEBUG_STRING(str, "%s\n", decodeErrors.c_str());
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    } else {
        this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
        this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
        this->options = singleDeviceBinary.buildOptions.str();

        if (false == singleDeviceBinary.debugData.empty()) {
            this->debugData = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
            this->debugDataSize = singleDeviceBinary.debugData.size();
        }

        this->isGeneratedByIgc = singleDeviceBinary.generator == NEO::GeneratorType::igc;

        bool rebuild = NEO::debugManager.flags.RebuildPrecompiledKernels.get() && irBinarySize != 0;
        rebuild |= !device->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv();

        if (rebuild && irBinarySize == 0) {
            driverHandle->clearErrorDescription();
            return ZE_RESULT_ERROR_INVALID_NATIVE_BINARY;
        }
        if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == rebuild)) {
            this->unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
            this->unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
            // If the Native Binary was an Archive, then packedTargetDeviceBinary will be the packed Binary for the Target Device.
            if (singleDeviceBinary.packedTargetDeviceBinary.size() > 0) {
                this->packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.packedTargetDeviceBinary.begin()), singleDeviceBinary.packedTargetDeviceBinary.size());
                this->packedDeviceBinarySize = singleDeviceBinary.packedTargetDeviceBinary.size();
            } else {
                this->packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
                this->packedDeviceBinarySize = archive.size();
            }
        }
    }

    if (nullptr == this->unpackedDeviceBinary) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", NEO::CompilerWarnings::recompiledFromIr.data());
        if (!shouldSuppressRebuildWarning) {
            updateBuildLog(NEO::CompilerWarnings::recompiledFromIr.str());
        }

        return buildFromSpirV(this->irBinary.get(), static_cast<uint32_t>(this->irBinarySize), this->options.c_str(), internalBuildOptions, nullptr);
    } else {
        if (processUnpackedBinary() != ZE_RESULT_SUCCESS) {
            driverHandle->clearErrorDescription();
            return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        }
        return ZE_RESULT_SUCCESS;
    }
}

ze_result_t ModuleTranslationUnit::processUnpackedBinary() {
    const auto driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (0 == unpackedDeviceBinarySize) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
    NEO::SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    binary.targetDevice = NEO::getTargetDevice(device->getNEODevice()->getRootDeviceEnvironment());
    std::string decodeErrors;
    std::string decodeWarnings;

    NEO::DecodeError decodeError;
    NEO::DeviceBinaryFormat singleDeviceBinaryFormat;
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings, gfxCoreHelper);
    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (NEO::DecodeError::success != decodeError) {
        CREATE_DEBUG_STRING(str, "%s\n", decodeErrors.c_str());
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    if (singleDeviceBinaryFormat == NEO::DeviceBinaryFormat::zebin && NEO::debugManager.flags.DumpZEBin.get() == 1) {
        dumpFileIncrement(reinterpret_cast<const char *>(blob.begin()), blob.size(), "dumped_zebin_module", ".elf");
    }

    processDebugData();

    size_t slmNeeded = NEO::getMaxInlineSlmNeeded(programInfo);
    size_t slmAvailable = 0U;
    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    slmAvailable = static_cast<size_t>(device->getDeviceInfo().localMemSize);
    deviceInfoConstants.computeUnitsUsedForScratch = static_cast<uint32_t>(device->getDeviceInfo().computeUnitsUsedForScratch);
    deviceInfoConstants.slmWindowSize = static_cast<uint32_t>(device->getDeviceInfo().localMemSize);
    if (NEO::requiresLocalMemoryWindowVA(programInfo)) {
        deviceInfoConstants.slmWindow = device->getNEODevice()->getExecutionEnvironment()->memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }

    if (slmNeeded > slmAvailable) {
        CREATE_DEBUG_STRING(str, "Size of SLM (%u) larger than available (%u)\n", static_cast<uint32_t>(slmNeeded), static_cast<uint32_t>(slmAvailable));
        driverHandle->setErrorDescription(std::string(str.get()));
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n",
                           static_cast<uint32_t>(slmNeeded), static_cast<uint32_t>(slmAvailable));
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto globalConstDataSize = programInfo.globalConstants.size + programInfo.globalConstants.zeroInitSize;
    if (globalConstDataSize != 0) {
        this->globalConstBuffer.reset(NEO::allocateGlobalsSurface(svmAllocsManager, *device->getNEODevice(), globalConstDataSize,
                                                                  programInfo.globalConstants.zeroInitSize, true, programInfo.linkerInput.get(), programInfo.globalConstants.initData));
    }

    auto globalVariablesDataSize = programInfo.globalVariables.size + programInfo.globalVariables.zeroInitSize;
    if (globalVariablesDataSize != 0) {
        this->globalVarBuffer.reset(NEO::allocateGlobalsSurface(svmAllocsManager, *device->getNEODevice(), globalVariablesDataSize,
                                                                programInfo.globalVariables.zeroInitSize, false, programInfo.linkerInput.get(), programInfo.globalVariables.initData));
    }

    for (auto &kernelInfo : this->programInfo.kernelInfos) {
        deviceInfoConstants.maxWorkGroupSize = gfxCoreHelper.calculateMaxWorkGroupSize(kernelInfo->kernelDescriptor, static_cast<uint32_t>(device->getDeviceInfo().maxWorkGroupSize), device->getNEODevice()->getRootDeviceEnvironment());
        kernelInfo->apply(deviceInfoConstants);
    }

    if (this->packedDeviceBinary != nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::SingleDeviceBinary singleDeviceBinary = {};
    singleDeviceBinary.targetDevice = NEO::getTargetDevice(device->getNEODevice()->getRootDeviceEnvironment());
    singleDeviceBinary.buildOptions = this->options;
    singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->unpackedDeviceBinary.get()), this->unpackedDeviceBinarySize);
    singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize);
    singleDeviceBinary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->debugData.get()), this->debugDataSize);
    std::string packWarnings;
    std::string packErrors;
    auto packedDeviceBinary = NEO::packDeviceBinary(singleDeviceBinary, packErrors, packWarnings);
    if (packedDeviceBinary.empty()) {
        driverHandle->clearErrorDescription();
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }
    this->packedDeviceBinary = makeCopy(packedDeviceBinary.data(), packedDeviceBinary.size());
    this->packedDeviceBinarySize = packedDeviceBinary.size();

    return ZE_RESULT_SUCCESS;
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

NEO::GraphicsAllocation *ModuleTranslationUnit::getGlobalConstBufferGA() const {
    return globalConstBuffer ? globalConstBuffer->getGraphicsAllocation() : nullptr;
}

NEO::GraphicsAllocation *ModuleTranslationUnit::getGlobalVarBufferGA() const {
    return globalVarBuffer ? globalVarBuffer->getGraphicsAllocation() : nullptr;
}

ModuleImp::ModuleImp(Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type)
    : device(device), translationUnit(std::make_unique<ModuleTranslationUnit>(device)),
      moduleBuildLog(moduleBuildLog), type(type) {
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &hwInfo = device->getHwInfo();
    this->isaAllocationPageSize = gfxCoreHelper.useSystemMemoryPlacementForISA(hwInfo) ? MemoryConstants::pageSize : MemoryConstants::pageSize64k;
    this->productFamily = hwInfo.platform.eProductFamily;
    this->metadataGeneration = std::make_unique<NEO::MetadataGeneration>();
}

ModuleImp::~ModuleImp() {
    for (auto &kernel : this->printfKernelContainer) {
        if (kernel.get() != nullptr) {
            destroyPrintfKernel(kernel->toHandle());
        }
    }
    this->kernelImmData.clear();
    if (this->sharedIsaAllocation) {
        auto neoDevice = this->device->getNEODevice();
        neoDevice->getIsaPoolAllocator().freeSharedIsaAllocation(this->sharedIsaAllocation.release());
    }
}

NEO::Zebin::Debug::Segments ModuleImp::getZebinSegments() {
    std::vector<NEO::Zebin::Debug::Segments::KernelNameIsaTupleT> kernels;
    for (const auto &data : kernelImmData) {
        NEO::Zebin::Debug::Segments::Segment segment = {data->getIsaGraphicsAllocation()->getGpuAddress(), data->getIsaGraphicsAllocation()->getUnderlyingBufferSize()};
        if (data->getIsaParentAllocation()) {
            segment.address += data->getIsaOffsetInParentAllocation();
            segment.size = data->getIsaSubAllocationSize();
        }
        kernels.push_back({data->getDescriptor().kernelMetadata.kernelName, segment});
    }

    ArrayRef<const uint8_t> strings = {reinterpret_cast<const uint8_t *>(translationUnit->programInfo.globalStrings.initData),
                                       translationUnit->programInfo.globalStrings.size};
    return NEO::Zebin::Debug::Segments(translationUnit->globalVarBuffer.get(), translationUnit->globalConstBuffer.get(), strings, kernels);
}

void ModuleImp::populateZebinExtendedArgsMetadata() {
    auto refBin = ArrayRef<const uint8_t>::fromAny(translationUnit->unpackedDeviceBinary.get(), translationUnit->unpackedDeviceBinarySize);
    this->metadataGeneration->callPopulateZebinExtendedArgsMetadataOnce(refBin, this->translationUnit->programInfo.kernelMiscInfoPos, this->translationUnit->programInfo.kernelInfos);
}

void ModuleImp::generateDefaultExtendedArgsMetadata() {
    this->metadataGeneration->callGenerateDefaultExtendedArgsMetadataOnce(this->translationUnit->programInfo.kernelInfos);
}

ze_result_t ModuleImp::initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) {
    bool linkageSuccessful = true;
    ze_result_t result = this->initializeTranslationUnit(desc, neoDevice);
    this->updateBuildLog(neoDevice);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    if (this->shouldBuildBeFailed(neoDevice)) {
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }
    if (result = this->initializeKernelImmutableData(); result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto refBin = ArrayRef<const uint8_t>::fromAny(translationUnit->unpackedDeviceBinary.get(), translationUnit->unpackedDeviceBinarySize);
    if (NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(refBin)) {
        isZebinBinary = true;
    }

    StackVec<NEO::GraphicsAllocation *, 32> moduleAllocs = getModuleAllocations();
    if (!moduleAllocs.empty()) {
        auto minGpuAddressAlloc = std::min_element(moduleAllocs.begin(), moduleAllocs.end(), [](const auto &alloc1, const auto &alloc2) { return alloc1->getGpuAddress() < alloc2->getGpuAddress(); });
        moduleLoadAddress = (*minGpuAddressAlloc)->getGpuAddress();
    }

    registerElfInDebuggerL0();

    checkIfPrivateMemoryPerDispatchIsNeeded();

    linkageSuccessful = this->linkBinary();

    linkageSuccessful &= populateHostGlobalSymbolsMap(this->translationUnit->programInfo.globalsDeviceToHostNameMap);
    this->updateBuildLog(neoDevice);

    if ((this->isFullyLinked && this->type == ModuleType::user) || (this->sharedIsaAllocation && this->type == ModuleType::builtin)) {
        this->transferIsaSegmentsToAllocation(neoDevice, nullptr);

        if (device->getL0Debugger()) {
            auto allocs = getModuleAllocations();
            device->getL0Debugger()->notifyModuleLoadAllocations(device->getNEODevice(), allocs);
            notifyModuleCreate();
        }
    }
    if (linkageSuccessful == false) {
        result = ZE_RESULT_ERROR_MODULE_LINK_FAILURE;
    }
    return result;
}

void ModuleImp::transferIsaSegmentsToAllocation(NEO::Device *neoDevice, const NEO::Linker::PatchableSegments *isaSegmentsForPatching) {
    const auto &productHelper = neoDevice->getProductHelper();
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();

    if (this->sharedIsaAllocation && this->kernelImmData.size()) {
        if (this->kernelImmData[0]->isIsaCopiedToAllocation()) {
            return;
        }

        const auto isaBufferSize = this->sharedIsaAllocation->getSize();
        DEBUG_BREAK_IF(isaBufferSize == 0);
        auto isaBuffer = std::vector<std::byte>(isaBufferSize);
        std::memset(isaBuffer.data(), 0x0, isaBufferSize);
        auto moduleOffset = sharedIsaAllocation->getOffset();
        for (auto &data : this->kernelImmData) {
            DEBUG_BREAK_IF(data->isIsaCopiedToAllocation());
            data->getIsaGraphicsAllocation()->setAubWritable(true, std::numeric_limits<uint32_t>::max());
            data->getIsaGraphicsAllocation()->setTbxWritable(true, std::numeric_limits<uint32_t>::max());

            auto [kernelHeapPtr, kernelHeapSize] = this->getKernelHeapPointerAndSize(data, isaSegmentsForPatching);
            auto isaOffset = data->getIsaOffsetInParentAllocation() - moduleOffset;
            memcpy_s(isaBuffer.data() + isaOffset, isaBufferSize - isaOffset, kernelHeapPtr, kernelHeapSize);
        }
        auto moduleAllocation = this->sharedIsaAllocation->getGraphicsAllocation();
        auto lock = this->sharedIsaAllocation->obtainSharedAllocationLock();
        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *moduleAllocation),
                                                              *neoDevice,
                                                              moduleAllocation,
                                                              moduleOffset,
                                                              isaBuffer.data(),
                                                              isaBuffer.size());

        if (neoDevice->getDefaultEngine().commandStreamReceiver->getType() != NEO::CommandStreamReceiverType::hardware) {
            neoDevice->getDefaultEngine().commandStreamReceiver->writeMemory(*moduleAllocation);
        }

        for (auto &data : kernelImmData) {
            data->setIsaCopiedToAllocation();
        }
    } else {
        for (auto &data : kernelImmData) {
            if (nullptr == data->getIsaGraphicsAllocation() || data->isIsaCopiedToAllocation()) {
                continue;
            }
            data->getIsaGraphicsAllocation()->setAubWritable(true, std::numeric_limits<uint32_t>::max());
            data->getIsaGraphicsAllocation()->setTbxWritable(true, std::numeric_limits<uint32_t>::max());

            auto [kernelHeapPtr, kernelHeapSize] = this->getKernelHeapPointerAndSize(data, isaSegmentsForPatching);
            NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *data->getIsaGraphicsAllocation()),
                                                                  *neoDevice,
                                                                  data->getIsaGraphicsAllocation(),
                                                                  0u,
                                                                  kernelHeapPtr,
                                                                  kernelHeapSize);
            data->setIsaCopiedToAllocation();
        }
    }
}

std::pair<const void *, size_t> ModuleImp::getKernelHeapPointerAndSize(const std::unique_ptr<KernelImmutableData> &kernelImmData,
                                                                       const NEO::Linker::PatchableSegments *isaSegmentsForPatching) {
    if (isaSegmentsForPatching) {
        auto &segments = *isaSegmentsForPatching;
        auto segmentId = &kernelImmData - &this->kernelImmData[0];
        return {segments[segmentId].hostPointer, segments[segmentId].segmentSize};
    } else {
        return {kernelImmData->getKernelInfo()->heapInfo.pKernelHeap,
                static_cast<size_t>(kernelImmData->getKernelInfo()->heapInfo.kernelHeapSize)};
    }
}

inline ze_result_t ModuleImp::initializeTranslationUnit(const ze_module_desc_t *desc, NEO::Device *neoDevice) {
    std::string buildOptions;
    std::string internalBuildOptions;

    if (desc->pNext) {
        const ze_base_desc_t *expDesc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
        if (expDesc->stype != ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (desc->format != ZE_MODULE_FORMAT_IL_SPIRV) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->builtFromSpirv = true;
        const ze_module_program_exp_desc_t *programExpDesc =
            reinterpret_cast<const ze_module_program_exp_desc_t *>(expDesc);
        std::vector<const char *> inputSpirVs;
        std::vector<uint32_t> inputModuleSizes;
        std::vector<const ze_module_constants_t *> specConstants;
        const ze_module_constants_t *firstSpecConstants = nullptr;

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
                if (i == 0) {
                    firstSpecConstants = specConstants[0];
                }
            }
            if (programExpDesc->pBuildFlags) {
                this->createBuildOptions(programExpDesc->pBuildFlags[i], tmpBuildOptions, tmpInternalBuildOptions);
                buildOptions = buildOptions + tmpBuildOptions;
                internalBuildOptions = internalBuildOptions + tmpInternalBuildOptions;
            }
        }
        // If the user passed in only 1 SPIRV, then fallback to standard build
        if (inputSpirVs.size() > 1) {
            this->precompiled = false;
            return this->translationUnit->staticLinkSpirV(inputSpirVs,
                                                          inputModuleSizes,
                                                          buildOptions.c_str(),
                                                          internalBuildOptions.c_str(),
                                                          specConstants);
        } else {
            this->precompiled = false;
            return this->translationUnit->buildFromSpirV(reinterpret_cast<const char *>(programExpDesc->pInputModules[0]),
                                                         inputModuleSizes[0],
                                                         buildOptions.c_str(),
                                                         internalBuildOptions.c_str(),
                                                         firstSpecConstants);
        }
    } else {
        std::string buildFlagsInput{desc->pBuildFlags != nullptr ? desc->pBuildFlags : ""};
        if (!this->verifyBuildOptions(buildFlagsInput)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        this->translationUnit->shouldSuppressRebuildWarning = NEO::CompilerOptions::extract(NEO::CompilerOptions::noRecompiledFromIr, buildFlagsInput);
        this->translationUnit->isBuiltIn = this->type == ModuleType::builtin ? true : false;
        this->createBuildOptions(buildFlagsInput.c_str(), buildOptions, internalBuildOptions);

        if (type == ModuleType::user) {
            if (NEO::debugManager.flags.InjectInternalBuildOptions.get() != "unk") {
                NEO::CompilerOptions::concatenateAppend(internalBuildOptions, NEO::debugManager.flags.InjectInternalBuildOptions.get());
            }

            if (NEO::debugManager.flags.InjectApiBuildOptions.get() != "unk") {
                NEO::CompilerOptions::concatenateAppend(buildOptions, NEO::debugManager.flags.InjectApiBuildOptions.get());
            }
        }

        if (desc->format == ZE_MODULE_FORMAT_NATIVE) {
            // Assume Symbol Generation Given Prebuilt Binary
            this->isFunctionSymbolExportEnabled = true;
            this->isGlobalSymbolExportEnabled = true;
            this->precompiled = true;
            return this->translationUnit->createFromNativeBinary(reinterpret_cast<const char *>(desc->pInputModule), desc->inputSize, internalBuildOptions.c_str());
        } else if (desc->format == ZE_MODULE_FORMAT_IL_SPIRV) {
            this->builtFromSpirv = true;
            this->precompiled = false;
            return this->translationUnit->buildFromSpirV(reinterpret_cast<const char *>(desc->pInputModule),
                                                         static_cast<uint32_t>(desc->inputSize),
                                                         buildOptions.c_str(),
                                                         internalBuildOptions.c_str(),
                                                         desc->pConstants);
        } else {
            this->precompiled = false;
            this->isFunctionSymbolExportEnabled = true;
            this->isGlobalSymbolExportEnabled = true;
            return this->translationUnit->buildExt(desc->format,
                                                   reinterpret_cast<const char *>(desc->pInputModule),
                                                   static_cast<uint32_t>(desc->inputSize),
                                                   buildOptions.c_str(),
                                                   internalBuildOptions.c_str());
        }
    }
}

inline bool ModuleImp::shouldBuildBeFailed(NEO::Device *neoDevice) {
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();
    auto containsStatefulAccess = NEO::AddressingModeHelper::containsStatefulAccess(translationUnit->programInfo.kernelInfos, false);
    auto isUserKernel = (type == ModuleType::user);
    auto isGeneratedByIgc = translationUnit->isGeneratedByIgc;
    return containsStatefulAccess &&
           isUserKernel &&
           NEO::AddressingModeHelper::failBuildProgramWithStatefulAccess(rootDeviceEnvironment) &&
           isGeneratedByIgc;
}

ze_result_t ModuleImp::initializeKernelImmutableData() {
    if (size_t kernelsCount = this->translationUnit->programInfo.kernelInfos.size(); kernelsCount > 0lu) {
        ze_result_t result;
        if (result = this->allocateKernelImmutableData(kernelsCount); result != ZE_RESULT_SUCCESS) {
            return result;
        }
        for (size_t i = 0lu; i < kernelsCount; i++) {
            result = kernelImmData[i]->initialize(this->translationUnit->programInfo.kernelInfos[i],
                                                  device,
                                                  device->getNEODevice()->getDeviceInfo().computeUnitsUsedForScratch,
                                                  this->translationUnit->globalConstBuffer.get(),
                                                  this->translationUnit->globalVarBuffer.get(),
                                                  this->type == ModuleType::builtin);
            if (result != ZE_RESULT_SUCCESS) {
                kernelImmData[i].reset();
                return result;
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::allocateKernelImmutableData(size_t kernelsCount) {
    if (this->kernelImmData.size() == kernelsCount) {
        return ZE_RESULT_SUCCESS;
    }

    this->kernelImmData.reserve(kernelsCount);
    for (size_t i = 0lu; i < kernelsCount; i++) {
        this->kernelImmData.emplace_back(new KernelImmutableData(this->device));
    }
    return this->setIsaGraphicsAllocations();
}

ze_result_t ModuleImp::setIsaGraphicsAllocations() {
    size_t kernelsCount = this->kernelImmData.size();

    auto kernelsChunks = std::vector<std::pair<size_t, size_t>>(kernelsCount);
    size_t kernelsIsaTotalSize = 0lu;
    for (auto i = 0lu; i < kernelsCount; i++) {
        auto kernelInfo = this->translationUnit->programInfo.kernelInfos[i];
        DEBUG_BREAK_IF(kernelInfo->heapInfo.kernelHeapSize == 0lu);
        DEBUG_BREAK_IF(!kernelInfo->heapInfo.pKernelHeap);
        auto chunkOffset = kernelsIsaTotalSize;
        auto chunkSize = this->computeKernelIsaAllocationAlignedSizeWithPadding(kernelInfo->heapInfo.kernelHeapSize, ((i + 1) == kernelsCount));
        kernelsIsaTotalSize += chunkSize;
        kernelsChunks[i] = {chunkOffset, chunkSize};
    }

    bool debuggerDisabled = (this->device->getL0Debugger() == nullptr);
    if (debuggerDisabled && kernelsIsaTotalSize <= isaAllocationPageSize) {
        auto neoDevice = this->device->getNEODevice();
        auto &isaAllocator = neoDevice->getIsaPoolAllocator();
        auto crossModuleAllocation = isaAllocator.requestGraphicsAllocationForIsa(this->type == ModuleType::builtin, kernelsIsaTotalSize);
        if (crossModuleAllocation == nullptr) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        this->sharedIsaAllocation.reset(crossModuleAllocation);
        for (auto i = 0lu; i < kernelsCount; i++) {
            auto [isaOffset, isaSize] = kernelsChunks[i];
            this->kernelImmData[i]->setIsaParentAllocation(this->sharedIsaAllocation->getGraphicsAllocation());
            this->kernelImmData[i]->setIsaSubAllocationOffset(this->sharedIsaAllocation->getOffset() + isaOffset);
            this->kernelImmData[i]->setIsaSubAllocationSize(isaSize);
        }
    } else {
        for (auto i = 0lu; i < kernelsCount; i++) {
            auto kernelInfo = this->translationUnit->programInfo.kernelInfos[i];
            if (auto allocation = this->allocateKernelsIsaMemory(kernelInfo->heapInfo.kernelHeapSize); allocation == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
            } else {
                this->kernelImmData[i]->setIsaPerKernelAllocation(allocation);
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

size_t ModuleImp::computeKernelIsaAllocationAlignedSizeWithPadding(size_t isaSize, bool lastKernel) {
    auto isaPadding = lastKernel ? this->device->getGfxCoreHelper().getPaddingForISAAllocation() : 0u;
    auto kernelStartPointerAlignment = this->device->getGfxCoreHelper().getKernelIsaPointerAlignment();
    auto isaAllocationSize = alignUp(isaPadding + isaSize, kernelStartPointerAlignment);
    return isaAllocationSize;
}

NEO::GraphicsAllocation *ModuleImp::allocateKernelsIsaMemory(size_t size) {
    auto allocType = (this->type == ModuleType::builtin ? NEO::AllocationType::kernelIsaInternal : NEO::AllocationType::kernelIsa);
    auto neoDevice = this->device->getNEODevice();

    NEO::AllocationProperties properties{neoDevice->getRootDeviceIndex(), size, allocType, neoDevice->getDeviceBitfield()};

    if (NEO::debugManager.flags.AlignLocalMemoryVaTo2MB.get() == 1) {
        properties.alignment = MemoryConstants::pageSize2M;
    }

    return neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
}

void ModuleImp::createDebugZebin() {
    auto refBin = ArrayRef<const uint8_t>::fromAny(translationUnit->unpackedDeviceBinary.get(), translationUnit->unpackedDeviceBinarySize);
    auto segments = getZebinSegments();
    auto debugZebin = NEO::Zebin::Debug::createDebugZebin(refBin, segments);

    translationUnit->debugDataSize = debugZebin.size();
    translationUnit->debugData.reset(new char[translationUnit->debugDataSize]);
    memcpy_s(translationUnit->debugData.get(), translationUnit->debugDataSize,
             debugZebin.data(), debugZebin.size());
}

const KernelImmutableData *ModuleImp::getKernelImmutableData(const char *kernelName) const {
    for (auto &data : kernelImmData) {
        if (data->getDescriptor().kernelMetadata.kernelName.compare(kernelName) == 0) {
            return data.get();
        }
    }
    return nullptr;
}

uint32_t ModuleImp::getMaxGroupSize(const NEO::KernelDescriptor &kernelDescriptor) const {
    return this->device->getGfxCoreHelper().calculateMaxWorkGroupSize(kernelDescriptor, static_cast<uint32_t>(this->device->getDeviceInfo().maxWorkGroupSize), device->getNEODevice()->getRootDeviceEnvironment());
}

void ModuleImp::createBuildOptions(const char *pBuildFlags, std::string &apiOptions, std::string &internalBuildOptions) {
    if (pBuildFlags != nullptr) {
        std::string buildFlags(pBuildFlags);

        apiOptions = pBuildFlags;
        NEO::CompilerOptions::applyAdditionalApiOptions(apiOptions);

        moveBuildOption(apiOptions, apiOptions, NEO::CompilerOptions::optDisable, BuildOptions::optDisable);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::greaterThan4gbBuffersRequired, BuildOptions::greaterThan4GbRequired);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::largeGrf, BuildOptions::optLargeRegisterFile);
        moveBuildOption(internalBuildOptions, apiOptions, NEO::CompilerOptions::autoGrf, BuildOptions::optAutoGrf);

        NEO::CompilerOptions::applyAdditionalInternalOptions(internalBuildOptions);

        moveOptLevelOption(apiOptions, apiOptions);
        moveProfileFlagsOption(apiOptions, apiOptions);
        this->isFunctionSymbolExportEnabled = moveBuildOption(apiOptions, apiOptions, BuildOptions::enableLibraryCompile, BuildOptions::enableLibraryCompile);
        this->isGlobalSymbolExportEnabled = moveBuildOption(apiOptions, apiOptions, BuildOptions::enableGlobalVariableSymbols, BuildOptions::enableGlobalVariableSymbols);

        if (getDevice()->getNEODevice()->getExecutionEnvironment()->isOneApiPvcWaEnv() == false) {
            NEO::CompilerOptions::concatenateAppend(internalBuildOptions, NEO::CompilerOptions::optDisableSendWarWa);
        }
        createBuildExtraOptions(apiOptions, internalBuildOptions);
    }
    if (NEO::ApiSpecificConfig::getBindlessMode(*device->getNEODevice())) {
        NEO::CompilerOptions::concatenateAppend(internalBuildOptions, NEO::CompilerOptions::bindlessMode.str());
    }
}

bool ModuleImp::moveOptLevelOption(std::string &dstOptionsSet, std::string &srcOptionSet) {
    const char optDelim = ' ';
    const char valDelim = '=';

    auto optInSrcPos = srcOptionSet.find(BuildOptions::optLevel.begin());
    if (std::string::npos == optInSrcPos) {
        return false;
    }

    std::string dstOptionStr(NEO::CompilerOptions::optLevel);
    auto valInSrcPos = srcOptionSet.find(valDelim, optInSrcPos);
    auto optInSrcEndPos = srcOptionSet.find(optDelim, optInSrcPos);
    if (std::string::npos == valInSrcPos) {
        return false;
    }
    dstOptionStr += srcOptionSet.substr(valInSrcPos + 1, (optInSrcEndPos - (valInSrcPos + 1)));
    srcOptionSet.erase(optInSrcPos, (optInSrcEndPos + 1 - optInSrcPos));
    NEO::CompilerOptions::concatenateAppend(dstOptionsSet, dstOptionStr);
    return true;
}

bool ModuleImp::moveProfileFlagsOption(std::string &dstOptionsSet, std::string &srcOptionSet) {
    const char optDelim = ' ';

    auto optInSrcPos = srcOptionSet.find(BuildOptions::profileFlags.begin());
    if (std::string::npos == optInSrcPos) {
        return false;
    }

    std::string dstOptionStr(BuildOptions::profileFlags);
    auto valInSrcPos = srcOptionSet.find(optDelim, optInSrcPos);
    auto optInSrcEndPos = srcOptionSet.find(optDelim, valInSrcPos + 1);
    if (std::string::npos == valInSrcPos) {
        return false;
    }
    std::string valStr = srcOptionSet.substr(valInSrcPos, (optInSrcEndPos - valInSrcPos));
    profileFlags = static_cast<uint32_t>(strtoul(valStr.c_str(), nullptr, 16));
    dstOptionStr += valStr;

    srcOptionSet.erase(optInSrcPos, (optInSrcEndPos + 1 - optInSrcPos));
    NEO::CompilerOptions::concatenateAppend(dstOptionsSet, dstOptionStr);
    return true;
}

void ModuleImp::updateBuildLog(NEO::Device *neoDevice) {
    if (this->moduleBuildLog) {
        moduleBuildLog->appendString(this->translationUnit->buildLog.c_str(), this->translationUnit->buildLog.size());
    }
}

ze_result_t ModuleImp::createKernel(const ze_kernel_desc_t *desc,
                                    ze_kernel_handle_t *kernelHandle) {
    ze_result_t res;
    const auto driverHandle = static_cast<DriverHandleImp *>((this->getDevice())->getDriverHandle());
    if (!isFullyLinked) {
        driverHandle->clearErrorDescription();
        return ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED;
    }
    auto kernel = Kernel::create(productFamily, this, desc, &res);

    if (res == ZE_RESULT_SUCCESS) {
        *kernelHandle = kernel->toHandle();
        if (kernel->getPrintfBufferAllocation() != nullptr) {
            this->printfKernelContainer.push_back(std::shared_ptr<Kernel>(kernel));
        }
    } else {
        driverHandle->clearErrorDescription();
    }

    return res;
}

std::weak_ptr<Kernel> ModuleImp::getPrintfKernelWeakPtr(ze_kernel_handle_t kernelHandle) const {
    std::lock_guard<std::mutex> lock(static_cast<DeviceImp *>(device)->printfKernelMutex);
    Kernel *kernel = Kernel::fromHandle(kernelHandle);
    auto it = std::find_if(this->printfKernelContainer.begin(), this->printfKernelContainer.end(), [&kernel](const auto &kernelSharedPtr) { return kernelSharedPtr.get() == kernel; });
    if (it == this->printfKernelContainer.end()) {
        return std::weak_ptr<Kernel>{};
    } else {
        return std::weak_ptr<Kernel>{*it};
    }
}

ze_result_t ModuleImp::destroyPrintfKernel(ze_kernel_handle_t kernelHandle) {
    std::lock_guard<std::mutex> lock(static_cast<DeviceImp *>(device)->printfKernelMutex);
    Kernel *kernel = Kernel::fromHandle(kernelHandle);
    auto it = std::find_if(this->printfKernelContainer.begin(), this->printfKernelContainer.end(), [&kernel](const auto &kernelSharedPtr) { return kernelSharedPtr.get() == kernel; });
    if (it == this->printfKernelContainer.end()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    it->reset();
    return ZE_RESULT_SUCCESS;
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

    if (nullptr == translationUnit->debugData.get() && isZebinBinary) {
        createDebugZebin();
    }
    if (pDebugData != nullptr) {
        if (*pDebugDataSize < translationUnit->debugDataSize) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        memcpy_s(pDebugData, *pDebugDataSize, translationUnit->debugData.get(), translationUnit->debugDataSize);
    }
    *pDebugDataSize = translationUnit->debugDataSize;
    return ZE_RESULT_SUCCESS;
}

void ModuleImp::copyPatchedSegments(const NEO::Linker::PatchableSegments &isaSegmentsForPatching) {
    if (this->translationUnit->programInfo.linkerInput && this->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        auto neoDevice = this->device->getNEODevice();
        auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();

        this->transferIsaSegmentsToAllocation(neoDevice, &isaSegmentsForPatching);

        for (auto &data : this->kernelImmData) {
            if (device->getL0Debugger()) {
                NEO::MemoryOperationsHandler *memoryOperationsIface = rootDeviceEnvironment.memoryOperationsInterface.get();
                auto allocation = data->getIsaGraphicsAllocation();
                if (memoryOperationsIface) {
                    memoryOperationsIface->makeResident(neoDevice, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1), false, false);
                }
            }
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
    Linker linker(*linkerInput, type == ModuleType::user);
    Linker::SegmentInfo globals;
    Linker::SegmentInfo constants;
    Linker::SegmentInfo exportedFunctions;
    Linker::SegmentInfo strings;
    SharedPoolAllocation *globalsForPatching = translationUnit->globalVarBuffer.get();
    SharedPoolAllocation *constantsForPatching = translationUnit->globalConstBuffer.get();

    auto &compilerProductHelper = this->device->getNEODevice()->getCompilerProductHelper();
    bool useFullAddress = compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo());

    if (globalsForPatching != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(globalsForPatching->getGpuAddress());
        globals.segmentSize = globalsForPatching->getSize();
    }
    if (constantsForPatching != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(constantsForPatching->getGpuAddress());
        constants.segmentSize = constantsForPatching->getSize();
    }
    if (translationUnit->programInfo.globalStrings.initData != nullptr) {
        strings.gpuAddress = reinterpret_cast<uintptr_t>(translationUnit->programInfo.globalStrings.initData);
        strings.segmentSize = translationUnit->programInfo.globalStrings.size;
    }
    if (linkerInput->getExportedFunctionsSegmentId() >= 0) {
        auto exportedFunctionHeapId = linkerInput->getExportedFunctionsSegmentId();
        this->exportedFunctionsSurface = this->kernelImmData[exportedFunctionHeapId]->getIsaGraphicsAllocation();
        auto offsetInParentAllocation = this->kernelImmData[exportedFunctionHeapId]->getIsaOffsetInParentAllocation();

        uintptr_t isaAddressToPatch = 0;
        if (useFullAddress) {
            isaAddressToPatch = static_cast<uintptr_t>(exportedFunctionsSurface->getGpuAddress() + offsetInParentAllocation);
        } else {
            isaAddressToPatch = static_cast<uintptr_t>(exportedFunctionsSurface->getGpuAddressToPatch() + offsetInParentAllocation);
        }

        exportedFunctions.gpuAddress = isaAddressToPatch;
        exportedFunctions.segmentSize = this->kernelImmData[exportedFunctionHeapId]->getIsaSize();
    }

    Linker::KernelDescriptorsT kernelDescriptors;

    if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        patchedIsaTempStorage.reserve(this->kernelImmData.size());
        kernelDescriptors.reserve(this->kernelImmData.size());
        for (size_t i = 0; i < kernelImmData.size(); i++) {
            auto kernelInfo = this->translationUnit->programInfo.kernelInfos.at(i);
            auto &kernHeapInfo = kernelInfo->heapInfo;
            const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.kernelHeapSize));
            uintptr_t isaAddressToPatch = 0;
            if (useFullAddress) {
                isaAddressToPatch = static_cast<uintptr_t>(kernelImmData.at(i)->getIsaGraphicsAllocation()->getGpuAddress() +
                                                           kernelImmData.at(i)->getIsaOffsetInParentAllocation());
            } else {
                isaAddressToPatch = static_cast<uintptr_t>(kernelImmData.at(i)->getIsaGraphicsAllocation()->getGpuAddressToPatch() +
                                                           kernelImmData.at(i)->getIsaOffsetInParentAllocation());
            }

            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), isaAddressToPatch, kernHeapInfo.kernelHeapSize});
            kernelDescriptors.push_back(&kernelInfo->kernelDescriptor);
        }
    }

    auto linkStatus = linker.link(globals, constants, exportedFunctions, strings,
                                  globalsForPatching, constantsForPatching,
                                  isaSegmentsForPatching, unresolvedExternalsInfo, this->device->getNEODevice(),
                                  translationUnit->programInfo.globalConstants.initData,
                                  translationUnit->programInfo.globalConstants.size,
                                  translationUnit->programInfo.globalVariables.initData,
                                  translationUnit->programInfo.globalVariables.size,
                                  kernelDescriptors, translationUnit->programInfo.externalFunctions);
    this->symbols = linker.extractRelocatedSymbols();
    if (LinkingStatus::linkedFully != linkStatus) {
        if (moduleBuildLog) {
            std::vector<std::string> kernelNames;
            for (const auto &kernelInfo : this->translationUnit->programInfo.kernelInfos) {
                kernelNames.push_back("kernel : " + kernelInfo->kernelDescriptor.kernelMetadata.kernelName);
            }
            auto error = constructLinkerErrorMessage(unresolvedExternalsInfo, kernelNames);
            moduleBuildLog->appendString(error.c_str(), error.size());
        }
        isFullyLinked = false;
        return LinkingStatus::linkedPartially == linkStatus;
    } else {
        copyPatchedSegments(isaSegmentsForPatching);
    }

    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->symbols));
    isFullyLinked = true;
    for (auto kernelId = 0u; kernelId < kernelImmData.size(); kernelId++) {
        auto &kernImmData = kernelImmData[kernelId];

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
    const auto driverHandle = static_cast<DriverHandleImp *>((this->getDevice())->getDriverHandle());
    // Check if the function is in the exported symbol table
    auto symbolIt = symbols.find(pFunctionName);
    if ((symbolIt != symbols.end()) && (symbolIt->second.symbol.segment == NEO::SegmentType::instructions)) {
        *pfnFunction = reinterpret_cast<void *>(symbolIt->second.gpuAddress);
    }
    // If the Function Pointer is not in the exported symbol table, then this function might be a kernel.
    // Check if the function name matches a kernel and return the gpu address to that function
    if (*pfnFunction == nullptr) {
        auto kernelImmData = this->getKernelImmutableData(pFunctionName);
        if (kernelImmData != nullptr) {
            auto isaAllocation = kernelImmData->getIsaGraphicsAllocation();
            *pfnFunction = reinterpret_cast<void *>(isaAllocation->getGpuAddress() + kernelImmData->getIsaOffsetInParentAllocation());
            // Ensure that any kernel in this module which uses this kernel module function pointer has access to the memory.
            for (auto &data : this->getKernelImmutableDataVector()) {
                if (data.get() != kernelImmData && data.get()->getIsaOffsetInParentAllocation() == 0lu) {
                    data.get()->getResidencyContainer().insert(data.get()->getResidencyContainer().end(), isaAllocation);
                }
            }
        }
    }

    if (*pfnFunction == nullptr) {
        if (!this->isFunctionSymbolExportEnabled) {
            CREATE_DEBUG_STRING(str, "Function Pointers Not Supported Without Compiler flag %s\n", BuildOptions::enableLibraryCompile.str().c_str());
            driverHandle->setErrorDescription(std::string(str.get()));
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Function Pointers Not Supported Without Compiler flag %s\n", BuildOptions::enableLibraryCompile.str().c_str());
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return ZE_RESULT_ERROR_INVALID_FUNCTION_NAME;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::getGlobalPointer(const char *pGlobalName, size_t *pSize, void **pPtr) {
    uint64_t address;
    size_t size;
    const auto driverHandle = static_cast<DriverHandleImp *>((this->getDevice())->getDriverHandle());

    auto hostSymbolIt = hostGlobalSymbolsMap.find(pGlobalName);
    if (hostSymbolIt != hostGlobalSymbolsMap.end()) {
        address = hostSymbolIt->second.address;
        size = hostSymbolIt->second.size;
    } else {
        auto deviceSymbolIt = symbols.find(pGlobalName);
        if (deviceSymbolIt != symbols.end()) {
            if (deviceSymbolIt->second.symbol.segment == NEO::SegmentType::instructions) {
                driverHandle->clearErrorDescription();
                return ZE_RESULT_ERROR_INVALID_GLOBAL_NAME;
            }
        } else {
            if (!this->isGlobalSymbolExportEnabled) {
                CREATE_DEBUG_STRING(str, "Global Pointers Not Supported Without Compiler flag %s\n", BuildOptions::enableGlobalVariableSymbols.str().c_str());
                driverHandle->setErrorDescription(std::string(str.get()));
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Global Pointers Not Supported Without Compiler flag %s\n", BuildOptions::enableGlobalVariableSymbols.str().c_str());
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            driverHandle->clearErrorDescription();
            return ZE_RESULT_ERROR_INVALID_GLOBAL_NAME;
        }
        address = deviceSymbolIt->second.gpuAddress;
        size = deviceSymbolIt->second.symbol.size;
    }
    if (pPtr) {
        *pPtr = reinterpret_cast<void *>(address);
    }
    if (pSize) {
        *pSize = size;
    }
    return ZE_RESULT_SUCCESS;
}

Module *Module::create(Device *device, const ze_module_desc_t *desc,
                       ModuleBuildLog *moduleBuildLog, ModuleType type, ze_result_t *result) {
    auto module = new ModuleImp(device, moduleBuildLog, type);

    *result = module->initialize(desc, device->getNEODevice());
    if (*result != ZE_RESULT_SUCCESS) {
        module->destroy();
        return nullptr;
    }

    return module;
}

ze_result_t ModuleImp::getKernelNames(uint32_t *pCount, const char **pNames) {
    auto &kernelImmData = this->getKernelImmutableDataVector();
    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(kernelImmData.size());
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > static_cast<uint32_t>(kernelImmData.size())) {
        *pCount = static_cast<uint32_t>(kernelImmData.size());
    }

    uint32_t outCount = 0;
    for (auto &kernelImmData : kernelImmData) {
        *(pNames + outCount) = kernelImmData->getDescriptor().kernelMetadata.kernelName.c_str();
        outCount++;
        if (outCount == *pCount) {
            break;
        }
    }

    return ZE_RESULT_SUCCESS;
}

void ModuleImp::checkIfPrivateMemoryPerDispatchIsNeeded() {
    size_t modulePrivateMemorySize = 0;
    auto neoDevice = this->device->getNEODevice();
    for (auto &data : this->kernelImmData) {
        if (0 == data->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize) {
            continue;
        }
        auto kernelPrivateMemorySize = NEO::KernelHelper::getPrivateSurfaceSize(data->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize,
                                                                                neoDevice->getDeviceInfo().computeUnitsUsedForScratch);
        modulePrivateMemorySize += kernelPrivateMemorySize;
    }

    this->allocatePrivateMemoryPerDispatch = false;
    if (modulePrivateMemorySize > 0U) {
        auto deviceBitfield = neoDevice->getDeviceBitfield();
        auto globalMemorySize = neoDevice->getRootDevice()->getGlobalMemorySize(static_cast<uint32_t>(deviceBitfield.to_ulong()));
        auto numSubDevices = deviceBitfield.count();
        this->allocatePrivateMemoryPerDispatch = modulePrivateMemorySize * numSubDevices > globalMemorySize;
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Private Memory Per Dispatch %d for modulePrivateMemorySize %zu subDevices %zu globalMemorySize %" PRIu64 "\n",
                           this->allocatePrivateMemoryPerDispatch, modulePrivateMemorySize, numSubDevices, globalMemorySize);
    }
}

ze_result_t ModuleImp::getProperties(ze_module_properties_t *pModuleProperties) {
    pModuleProperties->flags = 0;

    if (!unresolvedExternalsInfo.empty()) {
        pModuleProperties->flags |= ZE_MODULE_PROPERTY_FLAG_IMPORTS;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::inspectLinkage(
    ze_linkage_inspection_ext_desc_t *pInspectDesc,
    uint32_t numModules,
    ze_module_handle_t *phModules,
    ze_module_build_log_handle_t *phLog) {
    ModuleBuildLog *moduleLinkageLog = nullptr;
    moduleLinkageLog = ModuleBuildLog::create();
    *phLog = moduleLinkageLog->toHandle();

    for (auto i = 0u; i < numModules; i++) {
        auto moduleId = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));

        std::vector<std::string> unresolvedSymbolLogMessages;
        std::vector<std::string> resolvedSymbolLogMessages;
        std::vector<std::string> exportedSymbolLogMessages;
        std::stringstream logMessage;

        // Add All Reported Symbols as Exported Symbols
        for (auto const &symbolIt : moduleId->symbols) {
            logMessage.clear();
            logMessage << "Module <" << moduleId << ">: "
                       << " Exported Symbol <" << symbolIt.first << ">"
                       << "\n";
            exportedSymbolLogMessages.push_back(logMessage.str());
        }
        for (const auto &unresolvedExternal : moduleId->unresolvedExternalsInfo) {
            bool resolvedSymbol = false;
            for (auto resolvedModuleIndex = 0u; resolvedModuleIndex < numModules; resolvedModuleIndex++) {
                auto moduleHandle = static_cast<ModuleImp *>(Module::fromHandle(phModules[resolvedModuleIndex]));
                auto symbolIt = moduleHandle->symbols.find(unresolvedExternal.unresolvedRelocation.symbolName);
                if (symbolIt != moduleHandle->symbols.end() || moduleId->isFullyLinked) {
                    // Add Imported Symbols to the Imported Symbols Log if the modules included would resolve the symbol or this module has been fully linked
                    logMessage.clear();
                    logMessage << "Module <" << moduleId << ">: "
                               << " Imported Symbol <" << unresolvedExternal.unresolvedRelocation.symbolName << ">"
                               << "\n";
                    resolvedSymbolLogMessages.push_back(logMessage.str());
                    resolvedSymbol = true;
                    break;
                }
            }
            // Add the Symbol to the Unresolved Imports Log if none of the included modules would be able to resolve the symbol.
            if (!resolvedSymbol) {
                logMessage.clear();
                logMessage << "Module <" << moduleId << ">: "
                           << " Unresolved Imported Symbol <" << unresolvedExternal.unresolvedRelocation.symbolName << ">"
                           << "\n";
                unresolvedSymbolLogMessages.push_back(logMessage.str());
            }
        }

        if (pInspectDesc->flags & ZE_LINKAGE_INSPECTION_EXT_FLAG_IMPORTS) {
            for (auto logIndex = 0u; logIndex < resolvedSymbolLogMessages.size(); logIndex++) {
                moduleLinkageLog->appendString(resolvedSymbolLogMessages[logIndex].c_str(), resolvedSymbolLogMessages[logIndex].size());
            }
        }

        if (pInspectDesc->flags & ZE_LINKAGE_INSPECTION_EXT_FLAG_UNRESOLVABLE_IMPORTS) {
            for (auto logIndex = 0u; logIndex < unresolvedSymbolLogMessages.size(); logIndex++) {
                moduleLinkageLog->appendString(unresolvedSymbolLogMessages[logIndex].c_str(), unresolvedSymbolLogMessages[logIndex].size());
            }
        }

        if (pInspectDesc->flags & ZE_LINKAGE_INSPECTION_EXT_FLAG_EXPORTS) {
            for (auto logIndex = 0u; logIndex < exportedSymbolLogMessages.size(); logIndex++) {
                moduleLinkageLog->appendString(exportedSymbolLogMessages[logIndex].c_str(), exportedSymbolLogMessages[logIndex].size());
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ModuleImp::performDynamicLink(uint32_t numModules,
                                          ze_module_handle_t *phModules,
                                          ze_module_build_log_handle_t *phLinkLog) {
    std::map<void *, std::map<void *, void *>> dependencies;
    ModuleBuildLog *moduleLinkLog = nullptr;
    const auto driverHandle = static_cast<DriverHandleImp *>((this->getDevice())->getDriverHandle());
    if (phLinkLog) {
        moduleLinkLog = ModuleBuildLog::create();
        *phLinkLog = moduleLinkLog->toHandle();
    }
    for (auto i = 0u; i < numModules; i++) {
        auto moduleId = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
        // Add all provided Module's Exported Functions Surface to each Module to allow for all symbols
        // to be accessed from any module either directly thru Unresolved symbol resolution below or indirectly
        // thru function pointers or callbacks between the Modules.
        uint32_t functionSymbolExportEnabledCounter = 0;
        for (auto i = 0u; i < numModules; i++) {
            auto moduleHandle = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
            functionSymbolExportEnabledCounter += static_cast<uint32_t>(moduleHandle->isFunctionSymbolExportEnabled);
            if (nullptr != moduleHandle->exportedFunctionsSurface) {
                moduleId->importedSymbolAllocations.insert(moduleHandle->exportedFunctionsSurface);
            }
        }
        for (auto &data : moduleId->kernelImmData) {
            data->getResidencyContainer().insert(data->getResidencyContainer().end(), moduleId->importedSymbolAllocations.begin(),
                                                 moduleId->importedSymbolAllocations.end());
        }

        // If the Module is fully linked, this means no Unresolved Symbols Exist that require patching.
        if (moduleId->isFullyLinked) {
            continue;
        }

        // Resolve Unresolved Symbols in the Relocation Table between the Modules if Required.
        auto &isaSegmentsForPatching = moduleId->isaSegmentsForPatching;
        auto &patchedIsaTempStorage = moduleId->patchedIsaTempStorage;
        uint32_t numPatchedSymbols = 0u;
        auto &compilerProductHelper = this->device->getNEODevice()->getCompilerProductHelper();
        std::vector<std::string> unresolvedSymbolLogMessages;
        if (moduleId->translationUnit->programInfo.linkerInput && moduleId->translationUnit->programInfo.linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
            if (patchedIsaTempStorage.empty()) {
                patchedIsaTempStorage.reserve(moduleId->kernelImmData.size());
                for (size_t i = 0; i < kernelImmData.size(); i++) {
                    const auto kernelInfo = this->translationUnit->programInfo.kernelInfos.at(i);
                    auto &kernHeapInfo = kernelInfo->heapInfo;
                    const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
                    patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.kernelHeapSize));

                    uintptr_t isaAddressToPatch = 0;
                    if (compilerProductHelper.isHeaplessModeEnabled(this->device->getHwInfo())) {
                        isaAddressToPatch = static_cast<uintptr_t>(kernelImmData.at(i)->getIsaGraphicsAllocation()->getGpuAddress() +
                                                                   kernelImmData.at(i)->getIsaOffsetInParentAllocation());
                    } else {
                        isaAddressToPatch = static_cast<uintptr_t>(kernelImmData.at(i)->getIsaGraphicsAllocation()->getGpuAddressToPatch() +
                                                                   kernelImmData.at(i)->getIsaOffsetInParentAllocation());
                    }

                    isaSegmentsForPatching.push_back(NEO::Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), isaAddressToPatch, kernHeapInfo.kernelHeapSize});
                }
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

                        NEO::Linker::patchAddress(relocAddress, symbolIt->second.gpuAddress, unresolvedExternal.unresolvedRelocation);
                        numPatchedSymbols++;

                        if (moduleLinkLog) {
                            std::stringstream logMessage;
                            logMessage << " Successfully Resolved Thru Dynamic Link to Module <" << moduleHandle << ">";
                            unresolvedSymbolLogMessages.back().append(logMessage.str());
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
            driverHandle->clearErrorDescription();
            if (functionSymbolExportEnabledCounter == 0) {
                CREATE_DEBUG_STRING(str, "Dynamic Link Not Supported Without Compiler flag %s\n", BuildOptions::enableLibraryCompile.str().c_str());
                driverHandle->setErrorDescription(std::string(str.get()));
                PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Dynamic Link Not Supported Without Compiler flag %s\n", BuildOptions::enableLibraryCompile.str().c_str());
            }
            return ZE_RESULT_ERROR_MODULE_LINK_FAILURE;
        }
        moduleId->copyPatchedSegments(isaSegmentsForPatching);
        moduleId->isFullyLinked = true;
    }

    {
        NEO::ExternalFunctionInfosT externalFunctionInfos;
        NEO::FunctionDependenciesT extFuncDependencies;
        NEO::KernelDependenciesT kernelDependencies;
        NEO::KernelDescriptorMapT nameToKernelDescriptor;
        for (auto i = 0u; i < numModules; i++) {
            auto moduleId = static_cast<ModuleImp *>(Module::fromHandle(phModules[i]));
            auto &programInfo = moduleId->translationUnit->programInfo;

            auto toPtrVec = [](auto &inVec, auto &outPtrVec) {
                auto pos = outPtrVec.size();
                outPtrVec.resize(pos + inVec.size());
                for (size_t i = 0; i < inVec.size(); i++) {
                    outPtrVec[pos + i] = &inVec[i];
                }
            };
            toPtrVec(programInfo.externalFunctions, externalFunctionInfos);
            if (programInfo.linkerInput) {
                toPtrVec(programInfo.linkerInput->getFunctionDependencies(), extFuncDependencies);
                toPtrVec(programInfo.linkerInput->getKernelDependencies(), kernelDependencies);
            }

            for (auto &kernelInfo : programInfo.kernelInfos) {
                auto &kd = kernelInfo->kernelDescriptor;
                nameToKernelDescriptor[kd.kernelMetadata.kernelName] = &kd;
            }
        }
        auto error = NEO::resolveExternalDependencies(externalFunctionInfos, kernelDependencies, extFuncDependencies, nameToKernelDescriptor);
        if (error != NEO::RESOLVE_SUCCESS) {
            driverHandle->clearErrorDescription();
            return ZE_RESULT_ERROR_MODULE_LINK_FAILURE;
        }
    }
    return ZE_RESULT_SUCCESS;
}

bool ModuleImp::populateHostGlobalSymbolsMap(std::unordered_map<std::string, std::string> &devToHostNameMapping) {
    bool retVal = true;
    hostGlobalSymbolsMap.reserve(devToHostNameMapping.size());
    for (auto &[devName, hostName] : devToHostNameMapping) {
        auto findSymbolRes = symbols.find(devName);
        if (findSymbolRes != symbols.end()) {
            auto symbol = findSymbolRes->second;
            if (isDataSegment(symbol.symbol.segment)) {
                HostGlobalSymbol hostGlobalSymbol;
                hostGlobalSymbol.address = symbol.gpuAddress;
                hostGlobalSymbol.size = symbol.symbol.size;
                hostGlobalSymbolsMap[hostName] = hostGlobalSymbol;
            } else {
                translationUnit->buildLog.append("Error: Symbol with given device name: " + devName + " is not in .data segment.\n");
                retVal = false;
            }
        } else {
            translationUnit->buildLog.append("Error: No symbol found with given device name: " + devName + ".\n");
            retVal = false;
        }
    }
    return retVal;
}

ze_result_t ModuleImp::destroy() {
    notifyModuleDestroy();

    auto tempHandle = debugModuleHandle;
    auto tempDevice = device;

    auto rootDeviceIndex = getDevice()->getNEODevice()->getRootDeviceIndex();
    auto &executionEnvironment = getDevice()->getNEODevice()->getRootDeviceEnvironment().executionEnvironment;

    for (const auto &data : this->kernelImmData) {
        if (data->getIsaGraphicsAllocation()) {
            for (auto &engine : executionEnvironment.memoryManager->getRegisteredEngines(rootDeviceIndex)) {
                auto contextId = engine.osContext->getContextId();
                if (data->getIsaGraphicsAllocation()->isUsedByOsContext(contextId)) {
                    engine.commandStreamReceiver->registerInstructionCacheFlush();
                }
            }
        }
    }

    delete this;

    if (tempDevice->getL0Debugger() && tempHandle != 0) {
        tempDevice->getL0Debugger()->removeZebinModule(tempHandle);
    }

    return ZE_RESULT_SUCCESS;
}

void ModuleImp::registerElfInDebuggerL0() {
    auto debuggerL0 = device->getL0Debugger();

    if (this->type != ModuleType::user || !debuggerL0) {
        return;
    }

    if (isZebinBinary) {
        size_t debugDataSize = 0;
        getDebugInfo(&debugDataSize, nullptr);

        NEO::DebugData debugData; // pass debug zebin in vIsa field
        debugData.vIsa = reinterpret_cast<const char *>(translationUnit->debugData.get());
        debugData.vIsaSize = static_cast<uint32_t>(translationUnit->debugDataSize);
        this->debugElfHandle = debuggerL0->registerElf(&debugData);

        if (NEO::debugManager.flags.DebuggerLogBitmask.get() & NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_ELF) {
            dumpFileIncrement(debugData.vIsa, debugData.vIsaSize, "dumped_debug_module", ".elf");
        }

        StackVec<NEO::GraphicsAllocation *, 32> segmentAllocs;
        for (auto &data : kernelImmData) {
            segmentAllocs.push_back(data->getIsaGraphicsAllocation());
        }

        if (translationUnit->globalVarBuffer) {
            segmentAllocs.push_back(translationUnit->globalVarBuffer->getGraphicsAllocation());
        }
        if (translationUnit->globalConstBuffer) {
            segmentAllocs.push_back(translationUnit->globalConstBuffer->getGraphicsAllocation());
        }

        debuggerL0->attachZebinModuleToSegmentAllocations(segmentAllocs, this->debugModuleHandle, this->debugElfHandle);
    } else {
        for (auto &data : kernelImmData) {
            if (data->getKernelInfo()->kernelDescriptor.external.debugData.get()) {
                NEO::DebugData *notifyDebugData = data->getKernelInfo()->kernelDescriptor.external.debugData.get();
                NEO::DebugData relocatedDebugData;

                if (data->getKernelInfo()->kernelDescriptor.external.relocatedDebugData.get()) {
                    relocatedDebugData.genIsa = data->getKernelInfo()->kernelDescriptor.external.debugData->genIsa;
                    relocatedDebugData.genIsaSize = data->getKernelInfo()->kernelDescriptor.external.debugData->genIsaSize;
                    relocatedDebugData.vIsa = reinterpret_cast<char *>(data->getKernelInfo()->kernelDescriptor.external.relocatedDebugData.get());
                    relocatedDebugData.vIsaSize = data->getKernelInfo()->kernelDescriptor.external.debugData->vIsaSize;
                    notifyDebugData = &relocatedDebugData;
                }

                debuggerL0->registerElfAndLinkWithAllocation(notifyDebugData, data->getIsaGraphicsAllocation());
            }
        }
    }
}

void ModuleImp::notifyModuleCreate() {
    auto debuggerL0 = device->getL0Debugger();

    if (!debuggerL0) {
        return;
    }

    if (isZebinBinary) {
        size_t debugDataSize = 0;
        getDebugInfo(&debugDataSize, nullptr);
        UNRECOVERABLE_IF(!translationUnit->debugData);
        debuggerL0->notifyModuleCreate(translationUnit->debugData.get(), static_cast<uint32_t>(debugDataSize), moduleLoadAddress);
    } else {
        for (auto &data : kernelImmData) {
            auto debugData = data->getKernelInfo()->kernelDescriptor.external.debugData.get();
            auto relocatedDebugData = data->getKernelInfo()->kernelDescriptor.external.relocatedDebugData.get();

            if (debugData) {
                debuggerL0->notifyModuleCreate(relocatedDebugData ? reinterpret_cast<char *>(relocatedDebugData) : const_cast<char *>(debugData->vIsa), debugData->vIsaSize, data->getIsaGraphicsAllocation()->getGpuAddress());
            } else {
                debuggerL0->notifyModuleCreate(nullptr, 0, data->getIsaGraphicsAllocation()->getGpuAddress());
            }
        }
    }
}

void ModuleImp::notifyModuleDestroy() {
    auto debuggerL0 = device->getL0Debugger();

    if (!debuggerL0) {
        return;
    }

    if (isZebinBinary) {
        debuggerL0->notifyModuleDestroy(moduleLoadAddress);
    } else {
        for (auto &data : kernelImmData) {
            debuggerL0->notifyModuleDestroy(data->getIsaGraphicsAllocation()->getGpuAddress());
        }
    }
}

StackVec<NEO::GraphicsAllocation *, 32> ModuleImp::getModuleAllocations() {
    StackVec<NEO::GraphicsAllocation *, 32> allocs;
    if (auto isaParentAllocation = this->getKernelsIsaParentAllocation(); isaParentAllocation != nullptr) {
        allocs.push_back(isaParentAllocation);
    } else {
        // ISA allocations not optimized
        for (auto &data : kernelImmData) {
            allocs.push_back(data->getIsaGraphicsAllocation());
        }
    }

    if (translationUnit) {
        if (translationUnit->globalVarBuffer) {
            allocs.push_back(translationUnit->globalVarBuffer->getGraphicsAllocation());
        }
        if (translationUnit->globalConstBuffer) {
            allocs.push_back(translationUnit->globalConstBuffer->getGraphicsAllocation());
        }
    }
    return allocs;
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

NEO::GraphicsAllocation *ModuleImp::getKernelsIsaParentAllocation() const {
    if (!sharedIsaAllocation) {
        return nullptr;
    }
    return sharedIsaAllocation->getGraphicsAllocation();
}

} // namespace L0

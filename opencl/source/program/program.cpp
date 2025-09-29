/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_options_extra.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/metadata_generation.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"

namespace NEO {

Program::Program(Context *context, bool isBuiltIn, const ClDeviceVector &clDevicesIn) : executionEnvironment(*clDevicesIn[0]->getExecutionEnvironment()),
                                                                                        context(context),
                                                                                        clDevices(clDevicesIn),
                                                                                        isBuiltIn(isBuiltIn) {
    if (this->context && !this->isBuiltIn) {
        this->context->incRefInternal();
    }

    maxRootDeviceIndex = 0;

    for (const auto &device : clDevicesIn) {
        if (device->getRootDeviceIndex() > maxRootDeviceIndex) {
            maxRootDeviceIndex = device->getRootDeviceIndex();
        }
        deviceBuildInfos[device] = {};
        for (auto i = 0u; i < device->getNumSubDevices(); i++) {
            auto subDevice = device->getSubDevice(i);
            if (isDeviceAssociated(*subDevice)) {
                deviceBuildInfos[device].associatedSubDevices.push_back(subDevice);
            }
        }
    }

    buildInfos.resize(maxRootDeviceIndex + 1);
    debuggerInfos.resize(maxRootDeviceIndex + 1);
    metadataGeneration = std::make_unique<MetadataGeneration>();
}

std::string Program::getInternalOptions() const {
    auto pClDevice = clDevices[0];
    auto force32BitAddresses = pClDevice->getSharedDeviceInfo().force32BitAddresses;
    auto internalOptions = getOclVersionCompilerInternalOption(pClDevice->getEnabledClVersion());

    if (force32BitAddresses && !isBuiltIn) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
    }

    auto &hwInfo = pClDevice->getHardwareInfo();
    const auto &compilerProductHelper = pClDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto forceToStatelessRequired = compilerProductHelper.isForceToStatelessRequired();
    auto disableStatelessToStatefulOptimization = debugManager.flags.DisableStatelessToStatefulOptimization.get();

    if ((isBuiltIn && is32bit) || forceToStatelessRequired || disableStatelessToStatefulOptimization) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
    }

    if (NEO::ApiSpecificConfig::getBindlessMode(pClDevice->getDevice())) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessMode);
    }

    auto enableStatelessToStatefulWithOffset = pClDevice->getGfxCoreHelper().isStatelessToStatefulWithOffsetSupported();
    if (debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
        enableStatelessToStatefulWithOffset = debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
    }

    if (enableStatelessToStatefulWithOffset) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
    }

    const auto &productHelper = pClDevice->getProductHelper();
    if (productHelper.isForceEmuInt32DivRemSPWARequired(hwInfo)) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::forceEmuInt32DivRemSP);
    }

    if (hwInfo.capabilityTable.supportsImages) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableImageSupport);
    }

    if (pClDevice->getDevice().getExecutionEnvironment()->isFP64EmulationEnabled()) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableFP64GenEmu);
    }

    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::preserveVec3Type);
    auto isDebuggerActive = pClDevice->getDevice().getDebugger() != nullptr;
    CompilerOptions::concatenateAppend(internalOptions, compilerProductHelper.getCachingPolicyOptions(isDebuggerActive));
    CompilerOptions::applyExtraInternalOptions(internalOptions, hwInfo, compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);

    if (pClDevice->getDevice().getExecutionEnvironment()->isOneApiPvcWaEnv() == false) {
        NEO::CompilerOptions::concatenateAppend(internalOptions, NEO::CompilerOptions::optDisableSendWarWa);
    }
    return internalOptions;
}

Program::~Program() {
    for (auto i = 0u; i < buildInfos.size(); i++) {
        cleanCurrentKernelInfo(i);
    }

    for (const auto &buildInfo : buildInfos) {
        freeGlobalBufferAllocation(buildInfo.constantSurface);
        freeGlobalBufferAllocation(buildInfo.globalSurface);
    }

    notifyModuleDestroy();

    if (context && !isBuiltIn) {
        context->decRefInternal();
    }
}

void Program::freeGlobalBufferAllocation(const std::unique_ptr<NEO::SharedPoolAllocation> &globalBuffer) {
    if (!globalBuffer) {
        return;
    }

    auto graphicsAllocation = globalBuffer->getGraphicsAllocation();
    if (!graphicsAllocation) {
        return;
    }

    auto gpuAddress = reinterpret_cast<void *>(globalBuffer->getGpuAddress());

    for (const auto &device : clDevices) {
        if (auto usmPool = device->getDevice().getUsmConstantSurfaceAllocPool();
            usmPool && usmPool->isInPool(gpuAddress)) {
            [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
            DEBUG_BREAK_IF(!ret);
            return;
        }

        if (auto usmPool = device->getDevice().getUsmGlobalSurfaceAllocPool();
            usmPool && usmPool->isInPool(gpuAddress)) {
            [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
            DEBUG_BREAK_IF(!ret);
            return;
        }
    }

    if ((nullptr != context) && (nullptr != context->getSVMAllocsManager()) && (context->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(globalBuffer->getGpuAddress())))) {
        context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(globalBuffer->getGpuAddress()));
    } else {
        this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(globalBuffer->getGraphicsAllocation());
    }
}

cl_int Program::createProgramFromBinary(
    const void *pBinary,
    size_t binarySize, ClDevice &clDevice) {

    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    cl_int retVal = CL_INVALID_BINARY;
    if (pBinary == nullptr) {
        return retVal;
    }
    this->irBinary.reset();
    this->irBinarySize = 0U;
    this->isSpirV = false;
    this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset();
    this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = 0U;
    this->buildInfos[rootDeviceIndex].packedDeviceBinary.reset();
    this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = 0U;
    this->createdFrom = CreatedFrom::binary;

    ArrayRef<const uint8_t> archive(reinterpret_cast<const uint8_t *>(pBinary), binarySize);
    bool isSpirV = NEO::isSpirVBitcode(archive);

    if (isSpirV || NEO::isLlvmBitcode(archive)) {
        deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_INTERMEDIATE;
        retVal = processSpirBinary(archive.begin(), archive.size(), isSpirV);
    } else if (isAnyDeviceBinaryFormat(archive)) {
        deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_EXECUTABLE;
        this->isCreatedFromBinary = true;

        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto productAbbreviation = hardwarePrefix[hwInfo->platform.eProductFamily];

        TargetDevice targetDevice = getTargetDevice(rootDeviceEnvironment);
        std::string decodeErrors;
        std::string decodeWarnings;
        auto singleDeviceBinary = unpackSingleDeviceBinary(archive, ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice,
                                                           decodeErrors, decodeWarnings);
        if (decodeWarnings.empty() == false) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
        }

        bool singleDeviceBinaryEmpty = singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty();
        if (singleDeviceBinaryEmpty || (singleDeviceBinary.deviceBinary.empty() && debugManager.flags.DisableKernelRecompilation.get())) {
            retVal = CL_INVALID_BINARY;
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        } else {
            retVal = CL_SUCCESS;
            this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
            this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
            this->isSpirV = NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize));
            this->options = singleDeviceBinary.buildOptions.str();

            auto deviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
            auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(deviceBinary.get()), singleDeviceBinary.deviceBinary.size());
            SingleDeviceBinary binary = {};
            binary.deviceBinary = blob;
            binary.targetDevice = NEO::getTargetDevice(clDevice.getRootDeviceEnvironment());
            binary.generatorFeatureVersions = singleDeviceBinary.generatorFeatureVersions;

            auto &gfxCoreHelper = clDevice.getGfxCoreHelper();
            std::tie(decodedSingleDeviceBinary.decodeError, std::ignore) = NEO::decodeSingleDeviceBinary(decodedSingleDeviceBinary.programInfo,
                                                                                                         binary,
                                                                                                         decodedSingleDeviceBinary.decodeErrors,
                                                                                                         decodedSingleDeviceBinary.decodeWarnings,
                                                                                                         gfxCoreHelper);

            this->buildInfos[rootDeviceIndex].debugData = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
            this->buildInfos[rootDeviceIndex].debugDataSize = singleDeviceBinary.debugData.size();

            this->isGeneratedByIgc = singleDeviceBinary.generator == GeneratorType::igc;
            this->indirectDetectionVersion = singleDeviceBinary.generatorFeatureVersions.indirectMemoryAccessDetection;
            this->indirectAccessBufferMajorVersion = singleDeviceBinary.generatorFeatureVersions.indirectMemoryAccessDetection;

            bool rebuild = AddressingModeHelper::containsBindlessKernel(decodedSingleDeviceBinary.programInfo.kernelInfos);
            rebuild |= !clDevice.getDevice().getExecutionEnvironment()->isOneApiPvcWaEnv();

            bool flagRebuild = debugManager.flags.RebuildPrecompiledKernels.get();

            if (0u == this->irBinarySize) {
                if (flagRebuild) {
                    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Skip rebuild binary. Lack of IR, rebuild impossible.\n");
                }
                if (rebuild) {
                    return CL_INVALID_BINARY;
                }
            } else {
                rebuild |= flagRebuild;
            }

            if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == rebuild)) {
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = std::move(deviceBinary);
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
                this->buildInfos[rootDeviceIndex].packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
                this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = archive.size();
                this->decodedSingleDeviceBinary.isSet = true;
            } else {
                this->decodedSingleDeviceBinary.isSet = false;
                this->isCreatedFromBinary = false;
                this->requiresRebuild = true;
            }

            switch (singleDeviceBinary.format) {
            default:
                break;
            case DeviceBinaryFormat::oclLibrary:
                deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
                break;
            case DeviceBinaryFormat::oclCompiledObject:
                deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
                break;
            }
        }
    } else {
        retVal = this->createFromILExt(context, pBinary, binarySize);
    }

    return retVal;
}

cl_int Program::setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue) {
    if (!isSpirV) {
        return CL_INVALID_PROGRAM;
    }

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto &device = clDevices[0]->getDevice();

    if (!areSpecializationConstantsInitialized) {
        auto pCompilerInterface = device.getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        SpecConstantInfo specConstInfo;
        auto retVal = pCompilerInterface->getSpecConstantsInfo(device, ArrayRef<const char>(irBinary.get(), irBinarySize), specConstInfo);

        if (retVal != TranslationOutput::ErrorCode::success) {
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
    auto isTerminator = [](char c) { return c == '\0'; };
    auto errorString = ConstStringRef(pErrorString, errorStringSize).trimEnd(isTerminator);
    if (errorString.empty()) {
        return;
    }

    auto &buildLog = buildInfos[rootDeviceIndex].buildLog;
    if (false == buildLog.empty()) {
        buildLog.append("\n");
    }
    buildLog.append(errorString.begin(), errorString.end());
}

const char *Program::getBuildLog(uint32_t rootDeviceIndex) const {
    auto &currentLog = buildInfos[rootDeviceIndex].buildLog;
    return currentLog.c_str();
}

void Program::cleanCurrentKernelInfo(uint32_t rootDeviceIndex) {
    auto &buildInfo = buildInfos[rootDeviceIndex];
    for (auto &kernelInfo : buildInfo.kernelInfoArray) {
        if (kernelInfo->kernelAllocation) {
            // register cache flush in all csrs where kernel allocation was used
            for (auto &engine : this->executionEnvironment.memoryManager->getRegisteredEngines(rootDeviceIndex)) {
                auto contextId = engine.osContext->getContextId();
                if (kernelInfo->kernelAllocation->isUsedByOsContext(contextId)) {
                    engine.commandStreamReceiver->registerInstructionCacheFlush();
                }
            }

            if (executionEnvironment.memoryManager->isKernelBinaryReuseEnabled()) {
                auto lock = executionEnvironment.memoryManager->lockKernelAllocationMap();
                const auto &kernelName = kernelInfo->kernelDescriptor.kernelMetadata.kernelName;
                auto &storedBinaries = executionEnvironment.memoryManager->getKernelAllocationMap();
                auto kernelAllocations = storedBinaries.find(kernelName);
                if (kernelAllocations != storedBinaries.end()) {
                    kernelAllocations->second.reuseCounter--;
                    if (kernelAllocations->second.reuseCounter == 0) {
                        this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(kernelAllocations->second.kernelAllocation);
                        storedBinaries.erase(kernelAllocations);
                    }
                }
            } else {
                this->executionEnvironment.memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(kernelInfo->kernelAllocation);
            }
        }
        delete kernelInfo;
    }
    buildInfo.kernelInfoArray.clear();
    metadataGeneration.reset(new MetadataGeneration());
}

void Program::updateNonUniformFlag() {
    // Look for -cl-std=CL substring and extract value behind which can be 1.2 2.0 2.1 and convert to value
    auto pos = options.find(clStdOptionName);
    if (pos == std::string::npos) {
        programOptionVersion = 12u; // Default is 1.2
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

void Program::replaceDeviceBinary(std::unique_ptr<char[]> &&newBinary, size_t newBinarySize, uint32_t rootDeviceIndex) {
    if (isAnyPackedDeviceBinaryFormat(ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(newBinary.get()), newBinarySize))) {
        this->buildInfos[rootDeviceIndex].packedDeviceBinary = std::move(newBinary);
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = newBinarySize;
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.reset();
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = 0U;
    } else {
        this->buildInfos[rootDeviceIndex].packedDeviceBinary.reset();
        this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = 0U;
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = std::move(newBinary);
        this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = newBinarySize;
    }
}

cl_int Program::packDeviceBinary(ClDevice &clDevice) {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    if (nullptr != buildInfos[rootDeviceIndex].packedDeviceBinary) {
        return CL_SUCCESS;
    }

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];

    if (nullptr != this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()) {
        SingleDeviceBinary singleDeviceBinary = {};
        singleDeviceBinary.targetDevice = NEO::getTargetDevice(rootDeviceEnvironment);
        singleDeviceBinary.buildOptions = this->options;
        singleDeviceBinary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
        singleDeviceBinary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize);
        singleDeviceBinary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->buildInfos[rootDeviceIndex].debugData.get()), this->buildInfos[rootDeviceIndex].debugDataSize);

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
        if (deviceBuildInfos[&clDevice].programBinaryType == CL_PROGRAM_BINARY_TYPE_LIBRARY) {
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

void Program::setBuildStatus(cl_build_status status) {
    for (auto &deviceBuildInfo : deviceBuildInfos) {
        deviceBuildInfo.second.buildStatus = status;
    }
}
void Program::setBuildStatusSuccess(const ClDeviceVector &deviceVector, cl_program_binary_type binaryType) {
    for (const auto &device : deviceVector) {
        deviceBuildInfos[device].buildStatus = CL_BUILD_SUCCESS;
        if (deviceBuildInfos[device].programBinaryType != binaryType) {
            std::unique_lock<std::mutex> lock(lockMutex);
            clDevicesInProgram.push_back(device);
        }
        deviceBuildInfos[device].programBinaryType = binaryType;
        for (const auto &subDevice : deviceBuildInfos[device].associatedSubDevices) {
            deviceBuildInfos[subDevice].buildStatus = CL_BUILD_SUCCESS;
            if (deviceBuildInfos[subDevice].programBinaryType != binaryType) {
                std::unique_lock<std::mutex> lock(lockMutex);
                clDevicesInProgram.push_back(subDevice);
            }
            deviceBuildInfos[subDevice].programBinaryType = binaryType;
        }
    }
}

bool Program::isValidCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData) {
    return funcNotify != nullptr || userData == nullptr;
}

void Program::invokeCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData) {
    if (funcNotify != nullptr) {
        (*funcNotify)(this, userData);
    }
}

bool Program::isDeviceAssociated(const ClDevice &clDevice) const {
    return std::any_of(clDevices.begin(), clDevices.end(), [&](auto programDevice) { return programDevice == &clDevice; });
}

cl_int Program::processInputDevices(ClDeviceVector *&deviceVectorPtr, cl_uint numDevices, const cl_device_id *deviceList, const ClDeviceVector &allAvailableDevices) {
    if (deviceList == nullptr) {
        if (numDevices == 0) {
            deviceVectorPtr = const_cast<ClDeviceVector *>(&allAvailableDevices);
        } else {
            return CL_INVALID_VALUE;
        }

    } else {
        if (numDevices == 0) {
            return CL_INVALID_VALUE;
        } else {
            for (auto i = 0u; i < numDevices; i++) {
                auto device = castToObject<ClDevice>(deviceList[i]);
                if (!device || !std::any_of(allAvailableDevices.begin(), allAvailableDevices.end(), [&](auto validDevice) { return validDevice == device; })) {
                    return CL_INVALID_DEVICE;
                }
                deviceVectorPtr->push_back(device);
            }
        }
    }
    return CL_SUCCESS;
}

void Program::prependFilePathToOptions(const std::string &filename) {
    auto isCMCOptionUsed = CompilerOptions::contains(options, CompilerOptions::useCMCompiler);
    if (!filename.empty() && false == isCMCOptionUsed) {
        // Add "-s" flag first so it will be ignored by clang in case the options already have this flag set.
        options = CompilerOptions::generateSourcePath.str() + " " + CompilerOptions::wrapInQuotes(filename) + " " + options;
    }
}

const std::vector<ConstStringRef> Program::internalOptionsToExtract = {CompilerOptions::gtpinRera,
                                                                       CompilerOptions::defaultGrf,
                                                                       CompilerOptions::largeGrf,
                                                                       CompilerOptions::autoGrf,
                                                                       CompilerOptions::greaterThan4gbBuffersRequired,
                                                                       CompilerOptions::numThreadsPerEu};

bool Program::isFlagOption(ConstStringRef option) {
    if (option == CompilerOptions::numThreadsPerEu) {
        return false;
    }
    return true;
}

bool Program::isOptionValueValid(ConstStringRef option, ConstStringRef value) {
    if (option == CompilerOptions::numThreadsPerEu) {
        const auto &threadCounts = clDevices[0]->getSharedDeviceInfo().threadsPerEUConfigs;
        if (std::find(threadCounts.begin(), threadCounts.end(), atoi(value.data())) != threadCounts.end()) {
            return true;
        }
    }
    return false;
}

Context &Program::getContext() const {
    return *context;
}

Context *Program::getContextPtr() const {
    return context;
}

const ClDeviceVector &Program::getDevicesInProgram() const {
    if (clDevicesInProgram.empty()) {
        return clDevices;
    } else {
        return clDevicesInProgram;
    }
}

void Program::notifyModuleCreate() {
    if (isBuiltIn) {
        return;
    }

    for (const auto &device : clDevices) {
        if (device->getDevice().getL0Debugger()) {

            auto debuggerL0 = device->getDevice().getL0Debugger();
            auto rootDeviceIndex = device->getRootDeviceIndex();
            auto &buildInfo = this->buildInfos[rootDeviceIndex];
            auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);

            if (NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(refBin)) {

                createDebugZebin(rootDeviceIndex);
                NEO::DebugData debugData;
                debugData.vIsa = reinterpret_cast<const char *>(buildInfo.debugData.get());
                debugData.vIsaSize = static_cast<uint32_t>(buildInfo.debugDataSize);
                this->debuggerInfos[rootDeviceIndex].debugElfHandle = debuggerL0->registerElf(&debugData);

                auto allocs = getModuleAllocations(device->getRootDeviceIndex());
                debuggerL0->attachZebinModuleToSegmentAllocations(allocs, this->debuggerInfos[rootDeviceIndex].debugModuleHandle, this->debuggerInfos[rootDeviceIndex].debugElfHandle);
                device->getDevice().getL0Debugger()->notifyModuleLoadAllocations(&device->getDevice(), allocs);

                auto minGpuAddressAlloc = std::min_element(allocs.begin(), allocs.end(), [](const auto &alloc1, const auto &alloc2) { return alloc1->getGpuAddress() < alloc2->getGpuAddress(); });
                this->debuggerInfos[rootDeviceIndex].moduleLoadAddress = (*minGpuAddressAlloc)->getGpuAddress();
                debuggerL0->notifyModuleCreate(buildInfo.debugData.get(), static_cast<uint32_t>(buildInfo.debugDataSize), this->debuggerInfos[rootDeviceIndex].moduleLoadAddress);
            }
        }
    }
}

void Program::notifyModuleDestroy() {

    if (isBuiltIn) {
        return;
    }

    for (const auto &device : clDevices) {
        if (device->getDevice().getL0Debugger()) {
            auto debuggerL0 = device->getDevice().getL0Debugger();
            auto rootDeviceIndex = device->getRootDeviceIndex();
            auto tempHandle = this->debuggerInfos[rootDeviceIndex].debugModuleHandle;

            if (tempHandle != 0) {
                debuggerL0->removeZebinModule(tempHandle);
            }

            debuggerL0->notifyModuleDestroy(this->debuggerInfos[rootDeviceIndex].moduleLoadAddress);
        }
    }
}

StackVec<NEO::GraphicsAllocation *, 32> Program::getModuleAllocations(uint32_t rootIndex) {
    StackVec<NEO::GraphicsAllocation *, 32> allocs;
    auto &kernelInfoArray = buildInfos[rootIndex].kernelInfoArray;

    for (const auto &kernelInfo : kernelInfoArray) {
        allocs.push_back(kernelInfo->getGraphicsAllocation());
    }
    GraphicsAllocation *globalsForPatching = getGlobalSurfaceGA(rootIndex);
    GraphicsAllocation *constantsForPatching = getConstantSurfaceGA(rootIndex);

    if (globalsForPatching) {
        allocs.push_back(globalsForPatching);
    }
    if (constantsForPatching) {
        allocs.push_back(constantsForPatching);
    }
    return allocs;
}

void Program::callPopulateZebinExtendedArgsMetadataOnce(uint32_t rootDeviceIndex) {
    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    metadataGeneration->callPopulateZebinExtendedArgsMetadataOnce(refBin, buildInfo.kernelMiscInfoPos, buildInfo.kernelInfoArray);
}

void Program::callGenerateDefaultExtendedArgsMetadataOnce(uint32_t rootDeviceIndex) {
    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    metadataGeneration->callGenerateDefaultExtendedArgsMetadataOnce(buildInfo.kernelInfoArray);
}

NEO::SharedPoolAllocation *Program::getConstantSurface(uint32_t rootDeviceIndex) const {
    return buildInfos[rootDeviceIndex].constantSurface.get();
}

NEO::GraphicsAllocation *Program::getConstantSurfaceGA(uint32_t rootDeviceIndex) const {
    return buildInfos[rootDeviceIndex].constantSurface ? buildInfos[rootDeviceIndex].constantSurface->getGraphicsAllocation() : nullptr;
}

NEO::SharedPoolAllocation *Program::getGlobalSurface(uint32_t rootDeviceIndex) const {
    return buildInfos[rootDeviceIndex].globalSurface.get();
}

NEO::GraphicsAllocation *Program::getGlobalSurfaceGA(uint32_t rootDeviceIndex) const {
    return buildInfos[rootDeviceIndex].globalSurface ? buildInfos[rootDeviceIndex].globalSurface->getGraphicsAllocation() : nullptr;
}

NEO::GraphicsAllocation *Program::getExportedFunctionsSurface(uint32_t rootDeviceIndex) const {
    return buildInfos[rootDeviceIndex].exportedFunctionsSurface;
}

} // namespace NEO

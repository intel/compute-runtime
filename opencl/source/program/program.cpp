/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "program.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/kernel_info.h"

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
    kernelDebugEnabled = clDevices[0]->isDebuggerActive();
}
std::string Program::getInternalOptions() const {
    auto pClDevice = clDevices[0];
    auto force32BitAddressess = pClDevice->getSharedDeviceInfo().force32BitAddressess;
    auto internalOptions = getOclVersionCompilerInternalOption(pClDevice->getEnabledClVersion());

    if (force32BitAddressess && !isBuiltIn) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
    }

    auto &hwInfo = pClDevice->getHardwareInfo();
    const auto &compilerProductHelper = pClDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto forceToStatelessRequired = compilerProductHelper.isForceToStatelessRequired();
    auto disableStatelessToStatefulOptimization = DebugManager.flags.DisableStatelessToStatefulOptimization.get();

    if ((isBuiltIn && is32bit) || forceToStatelessRequired || disableStatelessToStatefulOptimization) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
    }

    if (ApiSpecificConfig::getBindlessConfiguration()) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessMode);
    }

    auto enableStatelessToStatefulWithOffset = pClDevice->getGfxCoreHelper().isStatelessToStatefulWithOffsetSupported();
    if (DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
        enableStatelessToStatefulWithOffset = DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
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

    CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::preserveVec3Type);
    auto isDebuggerActive = pClDevice->getDevice().isDebuggerActive() || pClDevice->getDevice().getDebugger() != nullptr;
    CompilerOptions::concatenateAppend(internalOptions, compilerProductHelper.getCachingPolicyOptions(isDebuggerActive));
    return internalOptions;
}

Program::~Program() {
    for (auto i = 0u; i < buildInfos.size(); i++) {
        cleanCurrentKernelInfo(i);
    }

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
    size_t binarySize, ClDevice &clDevice) {

    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
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
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
        }

        bool singleDeviceBinaryEmpty = singleDeviceBinary.intermediateRepresentation.empty() && singleDeviceBinary.deviceBinary.empty();
        if (singleDeviceBinaryEmpty || (singleDeviceBinary.deviceBinary.empty() && DebugManager.flags.DisableKernelRecompilation.get())) {
            retVal = CL_INVALID_BINARY;
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        } else {
            retVal = CL_SUCCESS;
            this->irBinary = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.intermediateRepresentation.begin()), singleDeviceBinary.intermediateRepresentation.size());
            this->irBinarySize = singleDeviceBinary.intermediateRepresentation.size();
            this->isSpirV = NEO::isSpirVBitcode(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary.get()), this->irBinarySize));
            this->options = singleDeviceBinary.buildOptions.str();
            if (singleDeviceBinary.format == NEO::DeviceBinaryFormat::Zebin) {
                this->options += " " + NEO::CompilerOptions::allowZebin.str();
            }

            this->buildInfos[rootDeviceIndex].debugData = makeCopy(reinterpret_cast<const char *>(singleDeviceBinary.debugData.begin()), singleDeviceBinary.debugData.size());
            this->buildInfos[rootDeviceIndex].debugDataSize = singleDeviceBinary.debugData.size();

            auto isVmeUsed = containsVmeUsage(this->buildInfos[rootDeviceIndex].kernelInfoArray);
            bool rebuild = isRebuiltToPatchtokensRequired(&clDevice.getDevice(), archive, this->options, this->isBuiltIn, isVmeUsed);
            rebuild |= DebugManager.flags.RebuildPrecompiledKernels.get();

            if (rebuild && 0u == this->irBinarySize) {
                return CL_INVALID_BINARY;
            }
            if ((false == singleDeviceBinary.deviceBinary.empty()) && (false == rebuild)) {
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinary.deviceBinary.size());
                this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = singleDeviceBinary.deviceBinary.size();
                this->buildInfos[rootDeviceIndex].packedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(archive.begin()), archive.size());
                this->buildInfos[rootDeviceIndex].packedDeviceBinarySize = archive.size();
            } else {
                this->isCreatedFromBinary = false;
                this->requiresRebuild = true;
            }

            switch (singleDeviceBinary.format) {
            default:
                break;
            case DeviceBinaryFormat::OclLibrary:
                deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
                break;
            case DeviceBinaryFormat::OclCompiledObject:
                deviceBuildInfos[&clDevice].programBinaryType = CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
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
    auto &device = clDevices[0]->getDevice();

    if (!areSpecializationConstantsInitialized) {
        auto pCompilerInterface = device.getCompilerInterface();
        if (nullptr == pCompilerInterface) {
            return CL_OUT_OF_HOST_MEMORY;
        }

        SpecConstantInfo specConstInfo;
        auto retVal = pCompilerInterface->getSpecConstantsInfo(device, ArrayRef<const char>(irBinary.get(), irBinarySize), specConstInfo);

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
    ConstStringRef errorString(pErrorString, errorStringSize);
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
            for (auto &engine : this->executionEnvironment.memoryManager->getRegisteredEngines()) {
                auto contextId = engine.osContext->getContextId();
                if (kernelInfo->kernelAllocation->isUsedByOsContext(contextId)) {
                    engine.commandStreamReceiver->registerInstructionCacheFlush();
                }
            }

            if (executionEnvironment.memoryManager->isKernelBinaryReuseEnabled()) {
                auto lock = executionEnvironment.memoryManager->lockKernelAllocationMap();
                auto kernelName = kernelInfo->kernelDescriptor.kernelMetadata.kernelName;
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

bool Program::containsVmeUsage(const std::vector<KernelInfo *> &kernelInfos) const {
    for (auto kernelInfo : kernelInfos) {
        if (kernelInfo->isVmeUsed()) {
            return true;
        }
    }
    return false;
}

void Program::disableZebinIfVmeEnabled(std::string &options, std::string &internalOptions, const std::string &sourceCode) {

    const char *vmeOptions[] = {"cl_intel_device_side_advanced_vme_enable",
                                "cl_intel_device_side_avc_vme_enable",
                                "cl_intel_device_side_vme_enable"};

    const char *vmeEnabledExtensions[] = {"cl_intel_motion_estimation : enable",
                                          "cl_intel_device_side_avc_motion_estimation : enable",
                                          "cl_intel_advanced_motion_estimation : enable"};

    auto containsVme = [](const auto &data, const auto &patterns) {
        for (const auto &pattern : patterns) {
            auto pos = data.find(pattern);
            if (pos != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    if (DebugManager.flags.DontDisableZebinIfVmeUsed.get() == true) {
        return;
    }

    if (containsVme(options, vmeOptions) || containsVme(sourceCode, vmeEnabledExtensions)) {
        auto pos = options.find(CompilerOptions::allowZebin.str());
        if (pos != std::string::npos) {
            options.erase(pos, pos + CompilerOptions::allowZebin.length());
        }
        internalOptions += " " + CompilerOptions::disableZebin.str();
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

const ClDeviceVector &Program::getDevicesInProgram() const {
    if (clDevicesInProgram.empty()) {
        return clDevices;
    } else {
        return clDevicesInProgram;
    }
}

} // namespace NEO

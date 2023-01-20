/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/time_measure_wrapper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/program/program.h"

#include "program_debug_data.h"

#include <algorithm>

using namespace iOpenCL;

namespace NEO {
extern bool familyEnabled[];

const KernelInfo *Program::getKernelInfo(
    const char *kernelName, uint32_t rootDeviceIndex) const {
    if (kernelName == nullptr) {
        return nullptr;
    }

    if (kernelName == NEO::Elf::SectionsNamesZebin::externalFunctions) {
        return nullptr;
    }

    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;

    auto it = std::find_if(kernelInfoArray.begin(), kernelInfoArray.end(),
                           [=](const KernelInfo *kInfo) { return (0 == strcmp(kInfo->kernelDescriptor.kernelMetadata.kernelName.c_str(), kernelName)); });

    return (it != kernelInfoArray.end()) ? *it : nullptr;
}

size_t Program::getNumKernels() const {
    auto numKernels = buildInfos[clDevices[0]->getRootDeviceIndex()].kernelInfoArray.size();
    auto usesExportedFunctions = (exportedFunctionsKernelId != std::numeric_limits<size_t>::max());
    if (usesExportedFunctions) {
        numKernels--;
    }
    return numKernels;
}

const KernelInfo *Program::getKernelInfo(size_t ordinal, uint32_t rootDeviceIndex) const {
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    if (exportedFunctionsKernelId == ordinal) {
        ordinal++;
    }
    DEBUG_BREAK_IF(ordinal >= kernelInfoArray.size());
    return kernelInfoArray[ordinal];
}

cl_int Program::linkBinary(Device *pDevice, const void *constantsInitData, size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize,
                           const ProgramInfo::GlobalSurfaceInfo &stringsInfo, std::vector<NEO::ExternalFunctionInfo> &extFuncInfos) {
    auto linkerInput = getLinkerInput(pDevice->getRootDeviceIndex());
    if (linkerInput == nullptr) {
        return CL_SUCCESS;
    }
    auto rootDeviceIndex = pDevice->getRootDeviceIndex();
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    buildInfos[rootDeviceIndex].constStringSectionData = stringsInfo;
    Linker linker(*linkerInput);
    Linker::SegmentInfo globals;
    Linker::SegmentInfo constants;
    Linker::SegmentInfo exportedFunctions;
    Linker::SegmentInfo strings;
    GraphicsAllocation *globalsForPatching = getGlobalSurface(rootDeviceIndex);
    GraphicsAllocation *constantsForPatching = getConstantSurface(rootDeviceIndex);
    if (globalsForPatching != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(globalsForPatching->getGpuAddress());
        globals.segmentSize = globalsForPatching->getUnderlyingBufferSize();
    }
    if (constantsForPatching != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(constantsForPatching->getGpuAddress());
        constants.segmentSize = constantsForPatching->getUnderlyingBufferSize();
    }
    if (stringsInfo.initData != nullptr) {
        strings.gpuAddress = reinterpret_cast<uintptr_t>(stringsInfo.initData);
        strings.segmentSize = stringsInfo.size;
    }
    if (linkerInput->getExportedFunctionsSegmentId() >= 0) {
        exportedFunctionsKernelId = static_cast<size_t>(linkerInput->getExportedFunctionsSegmentId());
        // Exported functions reside in instruction heap of one of kernels
        auto exportedFunctionHeapId = linkerInput->getExportedFunctionsSegmentId();
        buildInfos[rootDeviceIndex].exportedFunctionsSurface = kernelInfoArray[exportedFunctionHeapId]->getGraphicsAllocation();
        exportedFunctions.gpuAddress = static_cast<uintptr_t>(buildInfos[rootDeviceIndex].exportedFunctionsSurface->getGpuAddressToPatch());
        exportedFunctions.segmentSize = buildInfos[rootDeviceIndex].exportedFunctionsSurface->getUnderlyingBufferSize();
    }
    Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
    Linker::KernelDescriptorsT kernelDescriptors;
    if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        patchedIsaTempStorage.reserve(kernelInfoArray.size());
        kernelDescriptors.reserve(kernelInfoArray.size());
        for (const auto &kernelInfo : kernelInfoArray) {
            auto &kernHeapInfo = kernelInfo->heapInfo;
            const char *originalIsa = reinterpret_cast<const char *>(kernHeapInfo.pKernelHeap);
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.KernelHeapSize));
            DEBUG_BREAK_IF(nullptr == kernelInfo->getGraphicsAllocation());
            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), static_cast<uintptr_t>(kernelInfo->getGraphicsAllocation()->getGpuAddressToPatch()), kernHeapInfo.KernelHeapSize, kernelInfo->kernelDescriptor.kernelMetadata.kernelName});
            kernelDescriptors.push_back(&kernelInfo->kernelDescriptor);
        }
    }

    Linker::UnresolvedExternals unresolvedExternalsInfo;
    bool linkSuccess = LinkingStatus::LinkedFully == linker.link(globals, constants, exportedFunctions, strings,
                                                                 globalsForPatching, constantsForPatching,
                                                                 isaSegmentsForPatching, unresolvedExternalsInfo,
                                                                 pDevice, constantsInitData, constantsInitDataSize,
                                                                 variablesInitData, variablesInitDataSize,
                                                                 kernelDescriptors, extFuncInfos);
    setSymbols(rootDeviceIndex, linker.extractRelocatedSymbols());
    if (false == linkSuccess) {
        std::vector<std::string> kernelNames;
        for (const auto &kernelInfo : kernelInfoArray) {
            kernelNames.push_back("kernel : " + kernelInfo->kernelDescriptor.kernelMetadata.kernelName);
        }
        auto error = constructLinkerErrorMessage(unresolvedExternalsInfo, kernelNames);
        updateBuildLog(pDevice->getRootDeviceIndex(), error.c_str(), error.size());
        return CL_INVALID_BINARY;
    } else if (linkerInput->getTraits().requiresPatchingOfInstructionSegments) {
        for (auto kernelId = 0u; kernelId < kernelInfoArray.size(); kernelId++) {
            const auto &kernelInfo = kernelInfoArray[kernelId];
            auto &kernHeapInfo = kernelInfo->heapInfo;
            auto segmentId = &kernelInfo - &kernelInfoArray[0];
            auto &hwInfo = pDevice->getHardwareInfo();
            const auto &productHelper = pDevice->getProductHelper();
            MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelInfo->getGraphicsAllocation()),
                                                             *pDevice, kernelInfo->getGraphicsAllocation(), 0, isaSegmentsForPatching[segmentId].hostPointer,
                                                             static_cast<size_t>(kernHeapInfo.KernelHeapSize));
        }
    }
    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->getSymbols(pDevice->getRootDeviceIndex())));
    return CL_SUCCESS;
}

cl_int Program::processGenBinaries(const ClDeviceVector &clDevices, std::unordered_map<uint32_t, BuildPhase> &phaseReached) {
    cl_int retVal = CL_SUCCESS;
    for (auto &clDevice : clDevices) {
        if (BuildPhase::BinaryProcessing == phaseReached[clDevice->getRootDeviceIndex()]) {
            continue;
        }
        if (DebugManager.flags.PrintProgramBinaryProcessingTime.get()) {
            retVal = TimeMeasureWrapper::functionExecution(*this, &Program::processGenBinary, *clDevice);
        } else {
            retVal = processGenBinary(*clDevice);
        }

        if (retVal != CL_SUCCESS) {
            break;
        }
        phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::BinaryProcessing;
    }
    return retVal;
}

cl_int Program::processGenBinary(const ClDevice &clDevice) {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    if (nullptr == this->buildInfos[rootDeviceIndex].unpackedDeviceBinary) {
        ArrayRef<const uint8_t> archive(reinterpret_cast<uint8_t *>(this->buildInfos[rootDeviceIndex].packedDeviceBinary.get()), this->buildInfos[rootDeviceIndex].packedDeviceBinarySize);
        if (isAnyPackedDeviceBinaryFormat(archive)) {
            std::string outErrReason, outWarning;
            auto productAbbreviation = NEO::hardwarePrefix[clDevice.getHardwareInfo().platform.eProductFamily];
            NEO::TargetDevice targetDevice = NEO::getTargetDevice(clDevice.getRootDeviceEnvironment());

            auto singleDeviceBinary = unpackSingleDeviceBinary(archive, ConstStringRef(productAbbreviation, strlen(productAbbreviation)), targetDevice, outErrReason, outWarning);
            auto singleDeviceBinarySize = singleDeviceBinary.deviceBinary.size();

            this->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy<char>(reinterpret_cast<const char *>(singleDeviceBinary.deviceBinary.begin()), singleDeviceBinarySize);
            this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = singleDeviceBinarySize;
        } else {
            return CL_INVALID_BINARY;
        }
    }

    cleanCurrentKernelInfo(rootDeviceIndex);
    auto &buildInfo = buildInfos[rootDeviceIndex];

    if (buildInfo.constantSurface || buildInfo.globalSurface) {
        clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.constantSurface);
        clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.globalSurface);
        buildInfo.constantSurface = nullptr;
        buildInfo.globalSurface = nullptr;
    }

    ProgramInfo programInfo;
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    SingleDeviceBinary binary = {};
    binary.deviceBinary = blob;
    binary.targetDevice = NEO::getTargetDevice(clDevice.getRootDeviceEnvironment());
    std::string decodeErrors;
    std::string decodeWarnings;

    DecodeError decodeError;
    DeviceBinaryFormat singleDeviceBinaryFormat;
    auto &gfxCoreHelper = clDevice.getGfxCoreHelper();
    std::tie(decodeError, singleDeviceBinaryFormat) = NEO::decodeSingleDeviceBinary(programInfo, binary, decodeErrors, decodeWarnings, gfxCoreHelper);

    if (decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeWarnings.c_str());
    }

    if (DecodeError::Success != decodeError) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodeErrors.c_str());
        return CL_INVALID_BINARY;
    }

    return this->processProgramInfo(programInfo, clDevice);
}

cl_int Program::processProgramInfo(ProgramInfo &src, const ClDevice &clDevice) {
    auto rootDeviceIndex = clDevice.getRootDeviceIndex();
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    size_t slmNeeded = getMaxInlineSlmNeeded(src);
    size_t slmAvailable = 0U;
    NEO::DeviceInfoKernelPayloadConstants deviceInfoConstants;
    LinkerInput *linkerInput = nullptr;
    slmAvailable = static_cast<size_t>(clDevice.getSharedDeviceInfo().localMemSize);
    deviceInfoConstants.maxWorkGroupSize = static_cast<uint32_t>(clDevice.getSharedDeviceInfo().maxWorkGroupSize);
    deviceInfoConstants.computeUnitsUsedForScratch = clDevice.getSharedDeviceInfo().computeUnitsUsedForScratch;
    deviceInfoConstants.slmWindowSize = static_cast<uint32_t>(clDevice.getSharedDeviceInfo().localMemSize);
    if (requiresLocalMemoryWindowVA(src)) {
        deviceInfoConstants.slmWindow = this->executionEnvironment.memoryManager->getReservedMemory(MemoryConstants::slmWindowSize, MemoryConstants::slmWindowAlignment);
    }
    linkerInput = src.linkerInput.get();
    setLinkerInput(rootDeviceIndex, std::move(src.linkerInput));

    if (slmNeeded > slmAvailable) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n",
                           static_cast<uint32_t>(slmNeeded), static_cast<uint32_t>(slmAvailable));
        return CL_OUT_OF_RESOURCES;
    }

    kernelInfoArray = std::move(src.kernelInfos);
    auto svmAllocsManager = context ? context->getSVMAllocsManager() : nullptr;
    if (src.globalConstants.size != 0) {
        buildInfos[rootDeviceIndex].constantSurface = allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), src.globalConstants.size, true, linkerInput, src.globalConstants.initData);
    }

    buildInfos[rootDeviceIndex].globalVarTotalSize = src.globalVariables.size;

    if (src.globalVariables.size != 0) {
        buildInfos[rootDeviceIndex].globalSurface = allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), src.globalVariables.size, false, linkerInput, src.globalVariables.initData);
        if (clDevice.areOcl21FeaturesEnabled() == false) {
            buildInfos[rootDeviceIndex].globalVarTotalSize = 0u;
        }
    }
    buildInfos[rootDeviceIndex].kernelMiscInfoPos = src.kernelMiscInfoPos;

    for (auto &kernelInfo : kernelInfoArray) {
        cl_int retVal = CL_SUCCESS;
        if (kernelInfo->heapInfo.KernelHeapSize) {
            retVal = kernelInfo->createKernelAllocation(clDevice.getDevice(), isBuiltIn) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
        }

        if (retVal != CL_SUCCESS) {
            return retVal;
        }

        kernelInfo->apply(deviceInfoConstants);
    }

    return linkBinary(&clDevice.getDevice(), src.globalConstants.initData, src.globalConstants.size, src.globalVariables.initData,
                      src.globalVariables.size, src.globalStrings, src.externalFunctions);
}

void Program::processDebugData(uint32_t rootDeviceIndex) {
    if (this->buildInfos[rootDeviceIndex].debugData != nullptr) {
        auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
        SProgramDebugDataHeaderIGC *programDebugHeader = reinterpret_cast<SProgramDebugDataHeaderIGC *>(this->buildInfos[rootDeviceIndex].debugData.get());

        DEBUG_BREAK_IF(programDebugHeader->NumberOfKernels != kernelInfoArray.size());

        const SKernelDebugDataHeaderIGC *kernelDebugHeader = reinterpret_cast<SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(SProgramDebugDataHeaderIGC)));
        const char *kernelName = nullptr;
        const char *kernelDebugData = nullptr;

        for (uint32_t i = 0; i < programDebugHeader->NumberOfKernels; i++) {
            kernelName = reinterpret_cast<const char *>(ptrOffset(kernelDebugHeader, sizeof(SKernelDebugDataHeaderIGC)));

            auto kernelInfo = kernelInfoArray[i];
            UNRECOVERABLE_IF(kernelInfo->kernelDescriptor.kernelMetadata.kernelName.compare(0, kernelInfo->kernelDescriptor.kernelMetadata.kernelName.size(), kernelName) != 0);

            kernelDebugData = ptrOffset(kernelName, kernelDebugHeader->KernelNameSize);

            kernelInfo->debugData.vIsa = kernelDebugData;
            kernelInfo->debugData.genIsa = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes);
            kernelInfo->debugData.vIsaSize = kernelDebugHeader->SizeVisaDbgInBytes;
            kernelInfo->debugData.genIsaSize = kernelDebugHeader->SizeGenIsaDbgInBytes;

            kernelDebugData = ptrOffset(kernelDebugData, kernelDebugHeader->SizeVisaDbgInBytes + kernelDebugHeader->SizeGenIsaDbgInBytes);
            kernelDebugHeader = reinterpret_cast<const SKernelDebugDataHeaderIGC *>(kernelDebugData);
        }
    }
}

Debug::Segments Program::getZebinSegments(uint32_t rootDeviceIndex) {
    ArrayRef<const uint8_t> strings = {reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].constStringSectionData.initData),
                                       buildInfos[rootDeviceIndex].constStringSectionData.size};
    std::vector<std::pair<std::string_view, NEO::GraphicsAllocation *>> kernels;
    for (const auto &kernelInfo : buildInfos[rootDeviceIndex].kernelInfoArray)
        kernels.push_back({kernelInfo->kernelDescriptor.kernelMetadata.kernelName, kernelInfo->getGraphicsAllocation()});

    return Debug::Segments(getGlobalSurface(rootDeviceIndex), getConstantSurface(rootDeviceIndex), strings, kernels);
}

void Program::createDebugZebin(uint32_t rootDeviceIndex) {
    if (this->buildInfos[rootDeviceIndex].debugDataSize != 0) {
        return;
    }
    auto &debugDataRef = this->buildInfos[rootDeviceIndex].debugData;
    auto &debugDataSizeRef = this->buildInfos[rootDeviceIndex].debugDataSize;

    auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    auto segments = getZebinSegments(rootDeviceIndex);
    auto debugZebin = Debug::createDebugZebin(refBin, segments);

    debugDataSizeRef = debugZebin.size();
    debugDataRef.reset(new char[debugDataSizeRef]);
    memcpy_s(debugDataRef.get(), debugDataSizeRef,
             debugZebin.data(), debugZebin.size());
}

void Program::notifyDebuggerWithDebugData(ClDevice *clDevice) {
    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    if (NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(refBin)) {
        createDebugZebin(rootDeviceIndex);
        if (clDevice->getSourceLevelDebugger()) {
            NEO::DebugData debugData;
            debugData.vIsa = reinterpret_cast<const char *>(buildInfo.debugData.get());
            debugData.vIsaSize = static_cast<uint32_t>(buildInfo.debugDataSize);
            clDevice->getSourceLevelDebugger()->notifyKernelDebugData(&debugData, "debug_zebin", nullptr, 0);
        }
    } else {
        processDebugData(rootDeviceIndex);
        if (clDevice->getSourceLevelDebugger()) {
            for (auto &kernelInfo : buildInfo.kernelInfoArray) {
                clDevice->getSourceLevelDebugger()->notifyKernelDebugData(&kernelInfo->debugData,
                                                                          kernelInfo->kernelDescriptor.kernelMetadata.kernelName,
                                                                          kernelInfo->heapInfo.pKernelHeap,
                                                                          kernelInfo->heapInfo.KernelHeapSize);
            }
        }
    }
}

void Program::callPopulateZebinExtendedArgsMetadataOnce(uint32_t rootDeviceIndex) {
    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    auto extractAndDecodeMetadata = [&]() {
        auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
        if (false == NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(refBin)) {
            return;
        }
        std::string errors{}, warnings{};
        auto metadataString = extractZeInfoMetadataStringFromZebin(refBin, errors, warnings);
        auto decodeError = decodeAndPopulateKernelMiscInfo(buildInfo.kernelMiscInfoPos, buildInfo.kernelInfoArray, metadataString, errors, warnings);
        if (NEO::DecodeError::Success != decodeError) {
            PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Error in decodeAndPopulateKernelMiscInfo: %s\n", errors.c_str());
        }
    };
    std::call_once(extractAndDecodeMetadataOnce, extractAndDecodeMetadata);
}

void Program::callGenerateDefaultExtendedArgsMetadataOnce(uint32_t rootDeviceIndex) {
    auto ensureTypeNone = [](ArgTypeTraits &typeTraits) -> void {
        typeTraits.typeQualifiers.constQual = false;
        typeTraits.typeQualifiers.pipeQual = false;
        typeTraits.typeQualifiers.restrictQual = false;
        typeTraits.typeQualifiers.unknownQual = false;
        typeTraits.typeQualifiers.volatileQual = false;
    };

    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    auto generateDefaultMetadata = [&]() {
        for (const auto &kernelInfo : buildInfo.kernelInfoArray) {
            if (false == kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.empty()) {
                continue;
            }
            size_t argIndex = 0u;
            kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.resize(kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.size());
            for (auto &kernelArg : kernelInfo->kernelDescriptor.payloadMappings.explicitArgs) {
                ArgTypeMetadataExtended argMetadataExtended;
                auto &argTypeTraits = kernelArg.getTraits();
                argMetadataExtended.argName = std::string("arg" + std::to_string(argIndex));

                if (kernelArg.is<ArgDescriptor::ArgTValue>()) {
                    const auto &argAsValue = kernelArg.as<ArgDescValue>(false);
                    uint16_t maxSourceOffset = 0u, elemSize = 0u;
                    for (const auto &elem : argAsValue.elements) {
                        if (maxSourceOffset <= elem.sourceOffset) {
                            maxSourceOffset = elem.sourceOffset;
                            elemSize = elem.size;
                        }
                    }
                    if (maxSourceOffset != 0u) {
                        argMetadataExtended.type = std::string("__opaque_var;" + std::to_string(maxSourceOffset + elemSize));
                    } else {
                        argMetadataExtended.type = std::string("__opaque;" + std::to_string(elemSize));
                    }
                    ensureTypeNone(argTypeTraits);
                    argTypeTraits.addressQualifier = KernelArgMetadata::AddrPrivate;
                    argTypeTraits.accessQualifier = KernelArgMetadata::AccessNone;
                } else if (kernelArg.is<ArgDescriptor::ArgTPointer>()) {
                    const auto &argAsPtr = kernelArg.as<ArgDescPointer>(false);
                    argMetadataExtended.type = std::string("__opaque_ptr;" + std::to_string(argAsPtr.pointerSize));
                } else if (kernelArg.is<ArgDescriptor::ArgTImage>()) {
                    const auto &argAsImage = kernelArg.as<ArgDescImage>(false);
                    switch (argAsImage.imageType) {
                    case NEOImageType::ImageTypeBuffer:
                        argMetadataExtended.type = std::string("image1d_buffer_t");
                        break;
                    case NEOImageType::ImageType1D:
                        argMetadataExtended.type = std::string("image1d_t");
                        break;
                    case NEOImageType::ImageType1DArray:
                        argMetadataExtended.type = std::string("image1d_array_t");
                        break;
                    case NEOImageType::ImageType2DArray:
                        argMetadataExtended.type = std::string("image2d_array_t");
                        break;
                    case NEOImageType::ImageType3D:
                        argMetadataExtended.type = std::string("image3d_t");
                        break;
                    case NEOImageType::ImageType2DDepth:
                        argMetadataExtended.type = std::string("image2d_depth_t");
                        break;
                    case NEOImageType::ImageType2DArrayDepth:
                        argMetadataExtended.type = std::string("image2d_array_depth_t");
                        break;
                    case NEOImageType::ImageType2DMSAA:
                        argMetadataExtended.type = std::string("image2d_msaa_t");
                        break;
                    case NEOImageType::ImageType2DMSAADepth:
                        argMetadataExtended.type = std::string("image2d_msaa_depth_t");
                        break;
                    case NEOImageType::ImageType2DArrayMSAA:
                        argMetadataExtended.type = std::string("image2d_array_msaa_t");
                        break;
                    case NEOImageType::ImageType2DArrayMSAADepth:
                        argMetadataExtended.type = std::string("image2d_array_msaa_depth_t");
                        break;
                    default:
                        argMetadataExtended.type = std::string("image2d_t");
                        break;
                    }
                } else if (kernelArg.is<ArgDescriptor::ArgTSampler>()) {
                    argMetadataExtended.type = std::string("sampler_t");
                }
                kernelInfo->kernelDescriptor.explicitArgsExtendedMetadata.at(argIndex) = std::move(argMetadataExtended);
                argIndex++;
            }
        }
    };
    std::call_once(generateDefaultMetadataOnce, generateDefaultMetadata);
}

} // namespace NEO

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/zebin/debug_zebin.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/source/program/program_initialization.h"
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

    if (kernelName == NEO::Zebin::Elf::SectionNames::externalFunctions) {
        return nullptr;
    }

    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;

    auto it = std::find_if(kernelInfoArray.begin(), kernelInfoArray.end(),
                           [=](const KernelInfo *kInfo) { return (0 == strcmp(kInfo->kernelDescriptor.kernelMetadata.kernelName.c_str(), kernelName)); });

    return (it != kernelInfoArray.end()) ? *it : nullptr;
}

size_t Program::getNumKernels() const {
    auto pClDevice = this->getDevicesInProgram();
    auto rootDeviceIndex = pClDevice.at(0)->getRootDeviceIndex();
    auto numKernels = buildInfos[rootDeviceIndex].kernelInfoArray.size();
    auto usesExportedFunctions = (exportedFunctionsKernelId != std::numeric_limits<size_t>::max());
    if (usesExportedFunctions) {
        numKernels--;
    }
    return numKernels;
}

const KernelInfo *Program::getKernelInfo(size_t ordinal, uint32_t rootDeviceIndex) const {
    auto &kernelInfoArray = buildInfos[rootDeviceIndex].kernelInfoArray;
    if (exportedFunctionsKernelId <= ordinal) {
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
    SharedPoolAllocation *globalsForPatching = getGlobalSurface(rootDeviceIndex);
    SharedPoolAllocation *constantsForPatching = getConstantSurface(rootDeviceIndex);
    if (globalsForPatching != nullptr) {
        globals.gpuAddress = static_cast<uintptr_t>(globalsForPatching->getGpuAddress());
        globals.segmentSize = globalsForPatching->getSize();
    }
    if (constantsForPatching != nullptr) {
        constants.gpuAddress = static_cast<uintptr_t>(constantsForPatching->getGpuAddress());
        constants.segmentSize = constantsForPatching->getSize();
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
        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        if (compilerProductHelper.isHeaplessModeEnabled(pDevice->getHardwareInfo())) {
            exportedFunctions.gpuAddress = static_cast<uintptr_t>(buildInfos[rootDeviceIndex].exportedFunctionsSurface->getGpuAddress());
        } else {
            exportedFunctions.gpuAddress = static_cast<uintptr_t>(buildInfos[rootDeviceIndex].exportedFunctionsSurface->getGpuAddressToPatch());
        }
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
            patchedIsaTempStorage.push_back(std::vector<char>(originalIsa, originalIsa + kernHeapInfo.kernelHeapSize));
            DEBUG_BREAK_IF(nullptr == kernelInfo->getGraphicsAllocation());
            isaSegmentsForPatching.push_back(Linker::PatchableSegment{patchedIsaTempStorage.rbegin()->data(), static_cast<uintptr_t>(kernelInfo->getGraphicsAllocation()->getGpuAddressToPatch()), kernHeapInfo.kernelHeapSize});
            kernelDescriptors.push_back(&kernelInfo->kernelDescriptor);
        }
    }

    Linker::UnresolvedExternals unresolvedExternalsInfo;
    bool linkSuccess = LinkingStatus::linkedFully == linker.link(globals, constants, exportedFunctions, strings,
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
            auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
            const auto &productHelper = pDevice->getProductHelper();
            MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *kernelInfo->getGraphicsAllocation()),
                                                             *pDevice, kernelInfo->getGraphicsAllocation(), 0, isaSegmentsForPatching[segmentId].hostPointer,
                                                             static_cast<size_t>(kernHeapInfo.kernelHeapSize));
        }
    }
    DBG_LOG(PrintRelocations, NEO::constructRelocationsDebugMessage(this->getSymbols(pDevice->getRootDeviceIndex())));
    return CL_SUCCESS;
}

cl_int Program::processGenBinaries(const ClDeviceVector &clDevices, std::unordered_map<uint32_t, BuildPhase> &phaseReached) {
    cl_int retVal = CL_SUCCESS;
    for (auto &clDevice : clDevices) {
        if (BuildPhase::binaryProcessing == phaseReached[clDevice->getRootDeviceIndex()]) {
            continue;
        }
        if (debugManager.flags.PrintProgramBinaryProcessingTime.get()) {
            retVal = TimeMeasureWrapper::functionExecution(*this, &Program::processGenBinary, *clDevice);
        } else {
            retVal = processGenBinary(*clDevice);
        }

        if (retVal != CL_SUCCESS) {
            break;
        }
        phaseReached[clDevice->getRootDeviceIndex()] = BuildPhase::binaryProcessing;
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

            this->isGeneratedByIgc = singleDeviceBinary.generator == GeneratorType::igc;
            this->indirectDetectionVersion = singleDeviceBinary.generatorFeatureVersions.indirectMemoryAccessDetection;
            this->indirectAccessBufferMajorVersion = singleDeviceBinary.generatorFeatureVersions.indirectAccessBuffer;

        } else {
            return CL_INVALID_BINARY;
        }
    } else {
        if (NEO::debugManager.flags.DumpZEBin.get() == 1 && isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize))) {
            dumpFileIncrement(this->buildInfos[rootDeviceIndex].unpackedDeviceBinary.get(), this->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize, "dumped_zebin_module", ".elf");
        }
    }

    cleanCurrentKernelInfo(rootDeviceIndex);
    auto &buildInfo = buildInfos[rootDeviceIndex];

    if (buildInfo.constantSurface) {
        auto gpuAddress = reinterpret_cast<void *>(buildInfo.constantSurface->getGpuAddress());
        if (auto usmPool = clDevice.getDevice().getUsmConstantSurfaceAllocPool();
            usmPool && usmPool->isInPool(gpuAddress)) {
            [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
            DEBUG_BREAK_IF(!ret);
        } else {
            clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.constantSurface->getGraphicsAllocation());
        }
        buildInfo.constantSurface.reset();
    }
    if (buildInfo.globalSurface) {
        auto gpuAddress = reinterpret_cast<void *>(buildInfo.globalSurface->getGpuAddress());
        if (auto usmPool = clDevice.getDevice().getUsmGlobalSurfaceAllocPool();
            usmPool && usmPool->isInPool(gpuAddress)) {
            [[maybe_unused]] auto ret = usmPool->freeSVMAlloc(gpuAddress, false);
            DEBUG_BREAK_IF(!ret);
        } else {
            clDevice.getMemoryManager()->freeGraphicsMemory(buildInfo.globalSurface->getGraphicsAllocation());
        }
        buildInfo.globalSurface.reset();
    }

    if (!decodedSingleDeviceBinary.isSet) {
        decodedSingleDeviceBinary.programInfo = {};

        auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
        SingleDeviceBinary binary = {};
        binary.deviceBinary = blob;
        binary.targetDevice = NEO::getTargetDevice(clDevice.getRootDeviceEnvironment());

        auto &gfxCoreHelper = clDevice.getGfxCoreHelper();
        std::tie(decodedSingleDeviceBinary.decodeError, std::ignore) = NEO::decodeSingleDeviceBinary(decodedSingleDeviceBinary.programInfo, binary, decodedSingleDeviceBinary.decodeErrors, decodedSingleDeviceBinary.decodeWarnings, gfxCoreHelper);
    } else {
        decodedSingleDeviceBinary.isSet = false;
    }

    if (decodedSingleDeviceBinary.decodeWarnings.empty() == false) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodedSingleDeviceBinary.decodeWarnings.c_str());
    }

    if (DecodeError::success != decodedSingleDeviceBinary.decodeError) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s\n", decodedSingleDeviceBinary.decodeErrors.c_str());
        return CL_INVALID_BINARY;
    }

    return this->processProgramInfo(decodedSingleDeviceBinary.programInfo, clDevice);
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
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n",
                           static_cast<uint32_t>(slmNeeded), static_cast<uint32_t>(slmAvailable));
        return CL_OUT_OF_RESOURCES;
    }

    kernelInfoArray = std::move(src.kernelInfos);

    bool isBindlessKernelPresent = false;
    for (auto &kernelInfo : kernelInfoArray) {
        if (NEO::KernelDescriptor::isBindlessAddressingKernel(kernelInfo->kernelDescriptor)) {
            isBindlessKernelPresent = true;
            break;
        }
    }

    auto svmAllocsManager = context ? context->getSVMAllocsManager() : nullptr;
    auto globalConstDataSize = src.globalConstants.size + src.globalConstants.zeroInitSize;
    if (globalConstDataSize != 0) {
        buildInfos[rootDeviceIndex].constantSurface.reset(allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), globalConstDataSize, src.globalConstants.zeroInitSize, true, linkerInput, src.globalConstants.initData));
        if (isBindlessKernelPresent) {
            if (!clDevice.getMemoryManager()->allocateBindlessSlot(buildInfos[rootDeviceIndex].constantSurface->getGraphicsAllocation())) {
                return CL_OUT_OF_HOST_MEMORY;
            }
        }
    }

    auto globalVariablesDataSize = src.globalVariables.size + src.globalVariables.zeroInitSize;
    buildInfos[rootDeviceIndex].globalVarTotalSize = globalVariablesDataSize;
    if (globalVariablesDataSize != 0) {
        buildInfos[rootDeviceIndex].globalSurface.reset(allocateGlobalsSurface(svmAllocsManager, clDevice.getDevice(), globalVariablesDataSize, src.globalVariables.zeroInitSize, false, linkerInput, src.globalVariables.initData));
        if (isBindlessKernelPresent) {
            if (!clDevice.getMemoryManager()->allocateBindlessSlot(buildInfos[rootDeviceIndex].globalSurface->getGraphicsAllocation())) {
                return CL_OUT_OF_HOST_MEMORY;
            }
        }
        if (clDevice.areOcl21FeaturesEnabled() == false) {
            buildInfos[rootDeviceIndex].globalVarTotalSize = 0u;
        }
    }
    buildInfos[rootDeviceIndex].kernelMiscInfoPos = src.kernelMiscInfoPos;

    for (auto &kernelInfo : kernelInfoArray) {
        cl_int retVal = CL_SUCCESS;
        if (kernelInfo->heapInfo.kernelHeapSize) {
            retVal = kernelInfo->createKernelAllocation(clDevice.getDevice(), isBuiltIn) ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
        }

        if (retVal != CL_SUCCESS) {
            return retVal;
        }

        kernelInfo->apply(deviceInfoConstants);
    }

    indirectDetectionVersion = src.indirectDetectionVersion;
    indirectAccessBufferMajorVersion = src.indirectAccessBufferMajorVersion;

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

Zebin::Debug::Segments Program::getZebinSegments(uint32_t rootDeviceIndex) {
    ArrayRef<const uint8_t> strings = {reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].constStringSectionData.initData),
                                       buildInfos[rootDeviceIndex].constStringSectionData.size};
    std::vector<NEO::Zebin::Debug::Segments::KernelNameIsaTupleT> kernels;
    for (const auto &kernelInfo : buildInfos[rootDeviceIndex].kernelInfoArray) {

        NEO::Zebin::Debug::Segments::Segment segment = {static_cast<uintptr_t>(kernelInfo->getGraphicsAllocation()->getGpuAddress()), kernelInfo->getGraphicsAllocation()->getUnderlyingBufferSize()};
        kernels.push_back({kernelInfo->kernelDescriptor.kernelMetadata.kernelName, segment});
    }
    return Zebin::Debug::Segments(getGlobalSurface(rootDeviceIndex), getConstantSurface(rootDeviceIndex), strings, kernels);
}

void Program::createDebugZebin(uint32_t rootDeviceIndex) {
    if (this->buildInfos[rootDeviceIndex].debugDataSize != 0) {
        return;
    }
    auto &debugDataRef = this->buildInfos[rootDeviceIndex].debugData;
    auto &debugDataSizeRef = this->buildInfos[rootDeviceIndex].debugDataSize;

    auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfos[rootDeviceIndex].unpackedDeviceBinary.get()), buildInfos[rootDeviceIndex].unpackedDeviceBinarySize);
    auto segments = getZebinSegments(rootDeviceIndex);
    auto debugZebin = Zebin::Debug::createDebugZebin(refBin, segments);

    debugDataSizeRef = debugZebin.size();
    debugDataRef.reset(new char[debugDataSizeRef]);
    memcpy_s(debugDataRef.get(), debugDataSizeRef,
             debugZebin.data(), debugZebin.size());
}

void Program::createDebugData(ClDevice *clDevice) {
    auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    auto &buildInfo = this->buildInfos[rootDeviceIndex];
    auto refBin = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildInfo.unpackedDeviceBinary.get()), buildInfo.unpackedDeviceBinarySize);
    if (NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(refBin)) {
        createDebugZebin(rootDeviceIndex);
    } else {
        processDebugData(rootDeviceIndex);
    }
}

} // namespace NEO

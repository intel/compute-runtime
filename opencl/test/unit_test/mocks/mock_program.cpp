/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/program/program_info_from_patchtokens.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_compiler_interface.h"
#include "shared/test/unit_test/mocks/mock_graphics_allocation.h"

#include "opencl/source/context/context.h"
#include "opencl/source/program/create.inl"
#include "opencl/source/program/kernel_info.h"
#include "opencl/test/unit_test/helpers/ult_limits.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_compilers.h"

namespace NEO {
ClDeviceVector toClDeviceVector(ClDevice &clDevice) {
    ClDeviceVector deviceVector;
    deviceVector.push_back(&clDevice);
    return deviceVector;
}
ProgramInfo *GlobalMockSipProgram::globalSipProgramInfo;
Device *MockProgram::getDevicePtr() { return this->pDevice; }
int MockProgram::initInternalOptionsCalled = 0;

std::string MockProgram::getCachedFileName() const {
    auto hwInfo = this->context->getDevice(0)->getHardwareInfo();
    auto input = ArrayRef<const char>(this->sourceCode.c_str(), this->sourceCode.size());
    auto opts = ArrayRef<const char>(this->options.c_str(), this->options.size());
    auto internalOptions = getInitInternalOptions();
    auto internalOpts = ArrayRef<const char>(internalOptions.c_str(), internalOptions.size());
    return CompilerCache::getCachedFileName(hwInfo, input, opts, internalOpts);
}

void GlobalMockSipProgram::initSipProgramInfo() {
    globalSipProgramInfo = new ProgramInfo();
    std::vector<char> binary = MockCompilerInterface::getDummyGenBinary();
    auto blob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size());
    SingleDeviceBinary deviceBinary = {};
    deviceBinary.deviceBinary = blob;
    std::string decodeErrors;
    std::string decodeWarnings;

    NEO::decodeSingleDeviceBinary(*globalSipProgramInfo, deviceBinary, decodeErrors, decodeWarnings);
    auto sipAllocationStorage = alignedMalloc(globalSipProgramInfo->kernelInfos[0]->heapInfo.KernelHeapSize, MemoryConstants::pageSize);
    memcpy(sipAllocationStorage, globalSipProgramInfo->kernelInfos[0]->heapInfo.pKernelHeap, globalSipProgramInfo->kernelInfos[0]->heapInfo.KernelHeapSize);
    globalSipProgramInfo->kernelInfos[0]->heapInfo.pKernelHeap = sipAllocationStorage;
    globalSipProgramInfo->kernelInfos[0]->kernelAllocation = new MockGraphicsAllocation(sipAllocationStorage, globalSipProgramInfo->kernelInfos[0]->heapInfo.KernelHeapSize);
}
void GlobalMockSipProgram::shutDownSipProgramInfo() {
    deleteAllocation();
    delete globalSipProgramInfo;
}

void GlobalMockSipProgram::resetAllocation(GraphicsAllocation *allocation) {
    globalSipProgramInfo->kernelInfos[0]->kernelAllocation = allocation;
}
GraphicsAllocation *GlobalMockSipProgram::getAllocation() {
    return globalSipProgramInfo->kernelInfos[0]->kernelAllocation;
}
void GlobalMockSipProgram::deleteAllocation() {
    auto allocation = globalSipProgramInfo->kernelInfos[0]->kernelAllocation;
    alignedFree(allocation->getUnderlyingBuffer());
    delete allocation;
    globalSipProgramInfo->kernelInfos[0]->kernelAllocation = nullptr;
}

void GlobalMockSipProgram::resetAllocationState() {
    auto allocation = static_cast<MockGraphicsAllocation *>(globalSipProgramInfo->kernelInfos[0]->kernelAllocation);
    for (uint32_t index = 0u; index < allocation->usageInfos.size(); index++) {
        globalSipProgramInfo->kernelInfos[0]->kernelAllocation->releaseResidencyInOsContext(index);
    }
    allocation->resetInspectionIds();
}
ProgramInfo GlobalMockSipProgram::getSipProgramInfo() {
    ProgramInfo programInfo;
    programInfo.kernelInfos.push_back(new KernelInfo{});
    programInfo.kernelInfos[0]->heapInfo = GlobalMockSipProgram::globalSipProgramInfo->kernelInfos[0]->heapInfo;
    programInfo.kernelInfos[0]->kernelAllocation = GlobalMockSipProgram::getAllocation();
    return programInfo;
}
ProgramInfo GlobalMockSipProgram::getSipProgramInfoWithCustomBinary() {
    NEO::PatchTokenBinary::ProgramFromPatchtokens programTokens;
    programTokens.kernels.resize(1);

    const uint8_t isa[] = "kernel morphEUs()";
    const char name[] = "sip";

    SProgramBinaryHeader progHeader = {};
    progHeader.NumberOfKernels = 1;

    SKernelBinaryHeaderCommon kernHeader = {};
    kernHeader.KernelNameSize = sizeof(name);
    kernHeader.KernelHeapSize = sizeof(isa);
    kernHeader.KernelUnpaddedSize = sizeof(isa);

    programTokens.header = &progHeader;
    programTokens.kernels[0].header = &kernHeader;
    programTokens.kernels[0].isa = isa;
    programTokens.kernels[0].name = name;

    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programTokens);
    return programInfo;
}
} // namespace NEO

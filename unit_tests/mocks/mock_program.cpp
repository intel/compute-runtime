/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_program.h"

#include "core/compiler_interface/compiler_cache.h"
#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/helpers/hash.h"
#include "core/program/program_info_from_patchtokens.h"
#include "core/unit_tests/helpers/default_hw_info.h"
#include "runtime/context/context.h"
#include "runtime/program/create.inl"
#include "runtime/program/kernel_info.h"
#include "unit_tests/helpers/ult_limits.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

namespace NEO {
GlobalMockSipProgram *GlobalMockSipProgram::sipProgram;
ExecutionEnvironment GlobalMockSipProgram::executionEnvironment;

ClDevice *MockProgram::getDevicePtr() { return this->pDevice; }

std::string MockProgram::getCachedFileName() const {
    auto hwInfo = this->context->getDevice(0)->getHardwareInfo();
    auto input = ArrayRef<const char>(this->sourceCode.c_str(), this->sourceCode.size());
    auto opts = ArrayRef<const char>(this->options.c_str(), this->options.size());
    auto internalOpts = ArrayRef<const char>(this->internalOptions.c_str(), this->internalOptions.size());
    return CompilerCache::getCachedFileName(hwInfo, input, opts, internalOpts);
}
cl_int GlobalMockSipProgram::processGenBinary() {
    return CL_SUCCESS;
}

cl_int GlobalMockSipProgram::processGenBinaryOnce() {
    cl_int ret = Program::processGenBinary();
    sipAllocationStorage = alignedMalloc(this->kernelInfoArray[0]->heapInfo.pKernelHeader->KernelHeapSize, MemoryConstants::pageSize);
    this->kernelInfoArray[0]->kernelAllocation = new MockGraphicsAllocation(sipAllocationStorage, this->kernelInfoArray[0]->heapInfo.pKernelHeader->KernelHeapSize);
    return ret;
}
void GlobalMockSipProgram::resetAllocationState() {
    auto allocation = static_cast<MockGraphicsAllocation *>(this->kernelInfoArray[0]->kernelAllocation);
    for (uint32_t index = 0u; index < allocation->usageInfos.size(); index++) {
        this->kernelInfoArray[0]->kernelAllocation->releaseResidencyInOsContext(index);
    }
    allocation->resetInspectionIds();
}
void GlobalMockSipProgram::initSipProgram() {
    cl_int retVal = 0;
    std::vector<char> binary = MockCompilerInterface::getDummyGenBinary();
    executionEnvironment.setHwInfo(*platformDevices);
    executionEnvironment.prepareRootDeviceEnvironments(maxRootDeviceCount);
    executionEnvironment.calculateMaxOsContextCount();
    sipProgram = Program::createFromGenBinary<GlobalMockSipProgram>(executionEnvironment,
                                                                    nullptr,
                                                                    binary.data(),
                                                                    binary.size(),
                                                                    true,
                                                                    &retVal);
    DEBUG_BREAK_IF(retVal != 0);
    sipProgram->processGenBinaryOnce();
}

void GlobalMockSipProgram::resetAllocation(GraphicsAllocation *allocation) {
    this->kernelInfoArray[0]->kernelAllocation = allocation;
}
GraphicsAllocation *GlobalMockSipProgram::getAllocation() {
    return this->kernelInfoArray[0]->kernelAllocation;
}
void GlobalMockSipProgram::deleteAllocation() {
    delete this->kernelInfoArray[0]->kernelAllocation;
    alignedFree(sipAllocationStorage);
    this->kernelInfoArray[0]->kernelAllocation = nullptr;
}

void GlobalMockSipProgram::shutDownSipProgram() {
    sipProgram->deleteAllocation();

    delete sipProgram;
}

Program *GlobalMockSipProgram::getSipProgramWithCustomBinary() {
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

    Program *ret = new Program(executionEnvironment, nullptr, false);
    ret->processProgramInfo(programInfo);
    return ret;
}
} // namespace NEO

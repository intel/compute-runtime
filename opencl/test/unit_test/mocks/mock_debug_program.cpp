/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_debug_program.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"

#include "opencl/source/cl_device/cl_device.h"

#include "program_debug_data.h"

MockDebugProgram::MockDebugProgram(const NEO::ClDeviceVector &deviceVector) : NEO::Program(nullptr, false, deviceVector) {
    createdFrom = CreatedFrom::source;
    sourceCode = "__kernel void kernel(){}";
    prepareMockCompilerInterface(deviceVector[0]->getDevice());
}

void MockDebugProgram::debugNotify(const NEO::ClDeviceVector &deviceVector, std::unordered_map<uint32_t, BuildPhase> &phasesReached) {
    Program::debugNotify(deviceVector, phasesReached);
    wasDebuggerNotified = true;
}

void MockDebugProgram::createDebugZebin(uint32_t rootDeviceIndex) {
    Program::createDebugZebin(rootDeviceIndex);
    wasCreateDebugZebinCalled = true;
}

void MockDebugProgram::addKernelInfo(NEO::KernelInfo *inInfo, uint32_t rootDeviceIndex) {
    buildInfos[rootDeviceIndex].kernelInfoArray.push_back(inInfo);
}

void MockDebugProgram::processDebugData(uint32_t rootDeviceIndex) {
    Program::processDebugData(rootDeviceIndex);
    wasProcessDebugDataCalled = true;
}

cl_int MockDebugProgram::processGenBinary(const NEO::ClDevice &clDevice) {
    auto &kernelInfoArray = buildInfos[0].kernelInfoArray;
    kernelInfoArray.resize(1);
    if (kernelInfo == nullptr) {
        prepareKernelInfo();
    }
    kernelInfoArray[0] = kernelInfo;
    return CL_SUCCESS;
}

void MockDebugProgram::prepareKernelInfo() {
    kernelInfo = new NEO::KernelInfo;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "kernel";
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32U;
    prepareSSHForDebugSurface();
}

void MockDebugProgram::prepareSSHForDebugSurface() {
    kernelInfo->heapInfo.surfaceStateHeapSize = static_cast<uint32_t>(alignUp(64U + sizeof(int), 64U));
    kernelSsh = std::make_unique<char[]>(kernelInfo->heapInfo.surfaceStateHeapSize);
    memset(kernelSsh.get(), 0U, kernelInfo->heapInfo.surfaceStateHeapSize);
    kernelInfo->heapInfo.pSsh = kernelSsh.get();

    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0U;
    kernelInfo->kernelDescriptor.payloadMappings.bindingTable.numEntries = 1U;
    kernelInfo->kernelDescriptor.payloadMappings.bindingTable.tableOffset = 64U;
}

void MockDebugProgram::prepareMockCompilerInterface(NEO::Device &device) {
    auto mockCompilerInterface = std::make_unique<NEO::MockCompilerInterfaceCaptureBuildOptions>();
    this->compilerInterface = mockCompilerInterface.get();
    device.getRootDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->compilerInterface = std::move(mockCompilerInterface);

    compilerInterface->output.intermediateRepresentation.size = 32;
    compilerInterface->output.intermediateRepresentation.mem = std::make_unique<char[]>(32);

    compilerInterface->output.deviceBinary.size = 32;
    compilerInterface->output.deviceBinary.mem = std::make_unique<char[]>(32);

    constexpr char kernelName[] = "kernel";
    constexpr size_t isaSize = 8;
    constexpr size_t visaSize = 8;
    auto &debugData = compilerInterface->output.debugData;
    debugData.size = sizeof(iOpenCL::SProgramDebugDataHeaderIGC) + sizeof(iOpenCL::SKernelDebugDataHeaderIGC) + sizeof(kernelName) + isaSize + visaSize;
    debugData.mem = std::make_unique<char[]>(debugData.size);

    auto programDebugHeader = reinterpret_cast<iOpenCL::SProgramDebugDataHeaderIGC *>(debugData.mem.get());
    programDebugHeader->NumberOfKernels = 1;

    auto kernelDebugHeader = reinterpret_cast<iOpenCL::SKernelDebugDataHeaderIGC *>(ptrOffset(programDebugHeader, sizeof(iOpenCL::SProgramDebugDataHeaderIGC)));
    kernelDebugHeader->KernelNameSize = sizeof(kernelName);
    kernelDebugHeader->SizeGenIsaDbgInBytes = isaSize;
    kernelDebugHeader->SizeVisaDbgInBytes = visaSize;

    auto kernelNameDst = reinterpret_cast<char *>(ptrOffset(kernelDebugHeader, sizeof(iOpenCL::SKernelDebugDataHeader)));
    std::memcpy(kernelNameDst, kernelName, sizeof(kernelName));

    auto visa = ptrOffset(kernelNameDst, sizeof(kernelName));
    std::memset(visa, 0x10, visaSize);

    auto isa = ptrOffset(visa, visaSize);
    std::memset(isa, 0x20, isaSize);
}

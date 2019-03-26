/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_program.h"

#include "runtime/context/context.h"
#include "runtime/helpers/hash.h"
#include "runtime/program/create.inl"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

namespace NEO {
GlobalMockSipProgram *GlobalMockSipProgram::sipProgram;
ExecutionEnvironment GlobalMockSipProgram::executionEnvironment;
std::string MockProgram::getCachedFileName() const {
    auto hwInfo = this->context->getDevice(0)->getHardwareInfo();
    auto input = ArrayRef<const char>(this->sourceCode.c_str(), this->sourceCode.size());
    auto opts = ArrayRef<const char>(this->options.c_str(), this->options.size());
    auto internalOpts = ArrayRef<const char>(this->internalOptions.c_str(), this->internalOptions.size());
    return BinaryCache::getCachedFileName(hwInfo, input, opts, internalOpts);
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
    for (uint32_t index = 0u; index < maxOsContextCount; index++) {
        this->kernelInfoArray[0]->kernelAllocation->releaseResidencyInOsContext(index);
    }
    static_cast<MockGraphicsAllocation *>(this->kernelInfoArray[0]->kernelAllocation)->resetInspectionIds();
}
void GlobalMockSipProgram::initSipProgram() {
    cl_int retVal = 0;
    std::vector<char> binary = MockCompilerInterface::getDummyGenBinary();
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
    char binary[1024];
    char *pBinary = binary;
    auto totalSize = 0u;

    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)binary;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = platformDevices[0]->pPlatform->eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBinary += sizeof(SProgramBinaryHeader);
    totalSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBinary;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 4;
    pKHdr->PatchListSize = 0;
    pKHdr->KernelHeapSize = 16;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBinary += sizeof(SKernelBinaryHeaderCommon);
    totalSize += sizeof(SKernelBinaryHeaderCommon);
    char *pKernelBin = pBinary;
    strcpy_s(pBinary, 4, "sip");
    pBinary += pKHdr->KernelNameSize;
    totalSize += pKHdr->KernelNameSize;

    strcpy_s(pBinary, 18, "kernel morphEUs()");
    totalSize += pKHdr->KernelHeapSize;

    uint32_t kernelBinSize =
        pKHdr->DynamicStateHeapSize +
        pKHdr->GeneralStateHeapSize +
        pKHdr->KernelHeapSize +
        pKHdr->KernelNameSize +
        pKHdr->PatchListSize +
        pKHdr->SurfaceStateHeapSize;
    uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(pKernelBin), kernelBinSize);
    pKHdr->CheckSum = static_cast<uint32_t>(hashValue & 0xFFFFFFFF);

    auto errCode = CL_SUCCESS;
    auto program = Program::createFromGenBinary(executionEnvironment, nullptr, binary, totalSize, false, &errCode);
    UNRECOVERABLE_IF(errCode != CL_SUCCESS);
    errCode = program->processGenBinary();
    UNRECOVERABLE_IF(errCode != CL_SUCCESS);
    return program;
}
} // namespace NEO

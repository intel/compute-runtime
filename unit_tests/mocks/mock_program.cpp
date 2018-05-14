/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "runtime/helpers/hash.h"
#include "unit_tests/mocks/mock_compilers.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_program.h"

namespace OCLRT {
GlobalMockSipProgram *GlobalMockSipProgram::sipProgram;
uint64_t MockProgram::getHash() {
    return Hash::hash(genBinary, genBinarySize);
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
    this->kernelInfoArray[0]->kernelAllocation->residencyTaskCount = 0xffffffff;
    static_cast<MockGraphicsAllocation *>(this->kernelInfoArray[0]->kernelAllocation)->resetInspectionId();
}
void GlobalMockSipProgram::initSipProgram() {
    cl_int retVal = 0;
    std::vector<char> binary = MockCompilerInterface::getDummyGenBinary();
    sipProgram = Program::createFromGenBinary<GlobalMockSipProgram>(nullptr,
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
} // namespace OCLRT

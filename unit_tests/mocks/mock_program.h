/*
 * Copyright (c) 2017, Intel Corporation
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

#pragma once
#include "runtime/program/program.h"

namespace OCLRT {

class GraphicsAllocation;

////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockProgram : public Program {
  public:
    MockProgram() : Program() {}
    MockProgram(Context *context) : Program(context) {}
    ~MockProgram() {
        if (contextSet)
            context = nullptr;
    }
    const KernelInfo *getKernelInfo() { return &mockKernelInfo; }
    KernelInfo mockKernelInfo;
    void setBuildOptions(const char *buildOptions) {
        options = buildOptions != nullptr ? buildOptions : "";
    }
    std::string &getInternalOptions() { return internalOptions; };
    void setConstantSurface(GraphicsAllocation *gfxAllocation) {
        constantSurface = gfxAllocation;
    }
    void setGlobalSurface(GraphicsAllocation *gfxAllocation) {
        globalSurface = gfxAllocation;
    }
    cl_int createProgramFromBinary(const void *pBinary, size_t binarySize) override {
        return Program::createProgramFromBinary(pBinary, binarySize);
    }
    void setDevice(Device *device) {
        this->pDevice = device;
    };
    const KernelInfo *getBlockKernelInfo(size_t ordinal) {
        return blockKernelManager->getBlockKernelInfo(ordinal);
    }
    size_t getNumberOfBlocks() {
        return blockKernelManager->getCount();
    }
    void addBlockKernel(KernelInfo *blockInfo) {
        blockKernelManager->addBlockKernelInfo(blockInfo);
    }
    void separateBlockKernels() {
        Program::separateBlockKernels();
    }
    std::vector<KernelInfo *> &getKernelInfoArray() {
        return kernelInfoArray;
    }
    void addKernelInfo(KernelInfo *inInfo) {
        kernelInfoArray.push_back(inInfo);
    }
    std::vector<KernelInfo *> &getParentKernelInfoArray() {
        return parentKernelInfoArray;
    }
    std::vector<KernelInfo *> &getSubgroupKernelInfoArray() {
        return subgroupKernelInfoArray;
    }
    void setContext(Context *context) {
        this->context = context;
        contextSet = true;
    }

    void SetBuildStatus(cl_build_status st) { buildStatus = st; }
    void SetSourceCode(const char *ptr) { sourceCode = ptr; }
    void ClearOptions() { options = ""; }
    void SetCreatedFromBinary(bool createdFromBin) { isCreatedFromBinary = createdFromBin; }
    void ClearLog() { buildLog.clear(); }
    void SetGlobalVariableTotalSize(size_t globalVarSize) { globalVarTotalSize = globalVarSize; }
    void SetDevice(Device *pDev) { pDevice = pDev; }

    char *GetLLVMBinary() { return llvmBinary; }
    size_t GetLLVMBinarySize() { return llvmBinarySize; }
    void SetLLVMBinary(char *ptr) { llvmBinary = ptr; }
    void SetLLVMBinarySize(size_t bsz) { llvmBinarySize = bsz; }

    uint64_t getHash();

    bool contextSet = false;
};
} // namespace OCLRT

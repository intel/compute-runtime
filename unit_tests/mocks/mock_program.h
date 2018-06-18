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

#pragma once
#include "runtime/helpers/hash.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/string.h"
#include "runtime/program/program.h"

namespace OCLRT {

class GraphicsAllocation;

////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockProgram : public Program {
  public:
    using Program::isKernelDebugEnabled;

    MockProgram() : Program() {}
    MockProgram(Context *context, bool isBuiltinKernel) : Program(context, isBuiltinKernel) {}
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

    char *GetIrBinary() { return irBinary; }
    size_t GetIrBinarySize() { return irBinarySize; }
    void SetIrBinary(char *ptr, bool isSpirv) {
        irBinary = ptr;
        this->isSpirV = isSpirV;
    }
    void SetIrBinarySize(size_t bsz, bool isSpirv) {
        irBinarySize = bsz;
        this->isSpirV = isSpirV;
    }

    uint64_t getHash();
    void setAllowNonUniform(bool allow) {
        allowNonUniform = allow;
    }

    char *getDebugDataBinary(size_t &debugDataBinarySize) const {
        debugDataBinarySize = this->debugDataSize;
        return this->debugData;
    }

    Device *getDevicePtr() { return this->pDevice; }

    bool contextSet = false;
};

class GlobalMockSipProgram : public Program {
  public:
    using Program::Program;
    GlobalMockSipProgram() : Program() {
    }
    cl_int processGenBinary() override;
    cl_int processGenBinaryOnce();
    void resetAllocationState();
    void resetAllocation(GraphicsAllocation *allocation);
    void deleteAllocation();
    GraphicsAllocation *getAllocation();
    static void initSipProgram();
    static void shutDownSipProgram();
    static GlobalMockSipProgram *sipProgram;

  protected:
    void *sipAllocationStorage;
};

inline Program *getSipProgramWithCustomBinary() {
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
    pBinary += pKHdr->KernelHeapSize;
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
    auto program = Program::createFromGenBinary(nullptr, binary, totalSize, false, &errCode);
    UNRECOVERABLE_IF(errCode != CL_SUCCESS);
    errCode = program->processGenBinary();
    UNRECOVERABLE_IF(errCode != CL_SUCCESS);
    return program;
}

} // namespace OCLRT

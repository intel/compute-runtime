/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hash.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/string.h"
#include "runtime/program/program.h"

#include <string>

namespace OCLRT {

class GraphicsAllocation;

////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockProgram : public Program {
  public:
    using Program::createProgramFromBinary;
    using Program::getProgramCompilerVersion;
    using Program::isKernelDebugEnabled;
    using Program::rebuildProgramFromIr;
    using Program::resolveProgramBinary;
    using Program::updateNonUniformFlag;

    using Program::elfBinary;
    using Program::elfBinarySize;
    using Program::genBinary;
    using Program::genBinarySize;
    using Program::irBinary;
    using Program::irBinarySize;
    using Program::isProgramBinaryResolved;
    using Program::isSpirV;
    using Program::programBinaryType;

    using Program::sourceCode;

    MockProgram(ExecutionEnvironment &executionEnvironment) : Program(executionEnvironment) {}
    MockProgram(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltinKernel) : Program(executionEnvironment, context, isBuiltinKernel) {}
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

    std::string getCachedFileName() const;
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
    GlobalMockSipProgram(ExecutionEnvironment &executionEnvironment) : Program(executionEnvironment) {
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
    static Program *getSipProgramWithCustomBinary();

  protected:
    void *sipAllocationStorage;
    static ExecutionEnvironment executionEnvironment;
};

} // namespace OCLRT

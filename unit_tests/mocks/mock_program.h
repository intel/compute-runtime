/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hash.h"
#include "core/helpers/string.h"
#include "runtime/helpers/options.h"
#include "runtime/program/kernel_info.h"
#include "runtime/program/program.h"

#include "gmock/gmock.h"

#include <string>

namespace NEO {

class GraphicsAllocation;

////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockProgram : public Program {
  public:
    using Program::createProgramFromBinary;
    using Program::getKernelNamesString;
    using Program::internalOptionsToExtract;
    using Program::isKernelDebugEnabled;
    using Program::linkBinary;
    using Program::populateKernelInfo;
    using Program::prepareLinkerInputStorage;
    using Program::rebuildProgramFromIr;
    using Program::resolveProgramBinary;
    using Program::separateBlockKernels;
    using Program::updateNonUniformFlag;

    using Program::applyAdditionalOptions;
    using Program::areSpecializationConstantsInitialized;
    using Program::blockKernelManager;
    using Program::constantSurface;
    using Program::context;
    using Program::debugData;
    using Program::debugDataSize;
    using Program::elfBinary;
    using Program::elfBinarySize;
    using Program::exportedFunctionsSurface;
    using Program::extractInternalOptions;
    using Program::genBinary;
    using Program::genBinarySize;
    using Program::getKernelInfo;
    using Program::globalSurface;
    using Program::irBinary;
    using Program::irBinarySize;
    using Program::isProgramBinaryResolved;
    using Program::isSpirV;
    using Program::linkerInput;
    using Program::pDevice;
    using Program::programBinaryType;
    using Program::sourceCode;
    using Program::specConstantsIds;
    using Program::specConstantsSizes;
    using Program::specConstantsValues;
    using Program::symbols;

    template <typename... T>
    MockProgram(T &&... args) : Program(std::forward<T>(args)...) {
    }

    ~MockProgram() {
        if (contextSet)
            context = nullptr;
    }
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
    void setDevice(Device *device) {
        this->pDevice = device;
    };
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

    void SetIrBinary(char *ptr, bool isSpirv) {
        irBinary.reset(ptr);
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

    Device *getDevicePtr() { return this->pDevice; }

    bool isFlagOption(ConstStringRef option) override {
        if (isFlagOptionOverride != -1) {
            return (isFlagOptionOverride > 0);
        }
        return Program::isFlagOption(option);
    }

    bool isOptionValueValid(ConstStringRef option, ConstStringRef value) override {
        if (isOptionValueValidOverride != -1) {
            return (isOptionValueValidOverride > 0);
        }
        return Program::isOptionValueValid(option, value);
    }

    cl_int isHandled(const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram) const override {
        if (skipValidationOfBinary) {
            return CL_SUCCESS;
        }
        return Program::isHandled(decodedProgram);
    }

    bool contextSet = false;
    bool skipValidationOfBinary = false;
    int isFlagOptionOverride = -1;
    int isOptionValueValidOverride = -1;
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

class GMockProgram : public Program {
  public:
    using Program::Program;
    MOCK_METHOD0(appendKernelDebugOptions, bool(void));
};

} // namespace NEO

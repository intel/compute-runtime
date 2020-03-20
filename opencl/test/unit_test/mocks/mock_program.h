/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/string.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/program.h"

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
    using Program::internalOptionsToExtract;
    using Program::isKernelDebugEnabled;
    using Program::linkBinary;
    using Program::separateBlockKernels;
    using Program::updateNonUniformFlag;

    using Program::applyAdditionalOptions;
    using Program::areSpecializationConstantsInitialized;
    using Program::blockKernelManager;
    using Program::constantSurface;
    using Program::context;
    using Program::createdFrom;
    using Program::debugData;
    using Program::debugDataSize;
    using Program::exportedFunctionsSurface;
    using Program::extractInternalOptions;
    using Program::getKernelInfo;
    using Program::globalSurface;
    using Program::irBinary;
    using Program::irBinarySize;
    using Program::isSpirV;
    using Program::linkerInput;
    using Program::options;
    using Program::packDeviceBinary;
    using Program::packedDeviceBinary;
    using Program::packedDeviceBinarySize;
    using Program::pDevice;
    using Program::programBinaryType;
    using Program::sourceCode;
    using Program::specConstantsIds;
    using Program::specConstantsSizes;
    using Program::specConstantsValues;
    using Program::symbols;
    using Program::unpackedDeviceBinary;
    using Program::unpackedDeviceBinarySize;

    template <typename... T>
    MockProgram(T &&... args) : Program(std::forward<T>(args)...) {
    }

    ~MockProgram() override {
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

    Device *getDevicePtr();

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

    cl_int rebuildProgramFromIr() {
        this->isCreatedFromBinary = false;
        this->buildStatus = CL_BUILD_NONE;
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtins;
        auto &device = this->getDevice();
        return this->build(&device, this->options.c_str(), false, builtins);
    }

    bool contextSet = false;
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

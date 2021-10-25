/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/kernel_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/program/program.h"

#include "gmock/gmock.h"

#include <string>

namespace NEO {

class GraphicsAllocation;
ClDeviceVector toClDeviceVector(ClDevice &clDevice);
////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockProgram : public Program {
  public:
    using Program::allowNonUniform;
    using Program::applyAdditionalOptions;
    using Program::areSpecializationConstantsInitialized;
    using Program::blockKernelManager;
    using Program::buildInfos;
    using Program::context;
    using Program::createdFrom;
    using Program::createProgramFromBinary;
    using Program::debugData;
    using Program::debugDataSize;
    using Program::deviceBuildInfos;
    using Program::extractInternalOptions;
    using Program::getKernelInfo;
    using Program::internalOptionsToExtract;
    using Program::irBinary;
    using Program::irBinarySize;
    using Program::isSpirV;
    using Program::kernelDebugEnabled;
    using Program::linkBinary;
    using Program::options;
    using Program::packDeviceBinary;
    using Program::Program;
    using Program::separateBlockKernels;
    using Program::setBuildStatus;
    using Program::sourceCode;
    using Program::specConstantsIds;
    using Program::specConstantsSizes;
    using Program::specConstantsValues;
    using Program::updateNonUniformFlag;

    MockProgram(const ClDeviceVector &deviceVector) : Program(nullptr, false, deviceVector) {
    }

    ~MockProgram() override {
        if (contextSet)
            context = nullptr;
    }
    KernelInfo mockKernelInfo;
    void setBuildOptions(const char *buildOptions) {
        options = buildOptions != nullptr ? buildOptions : "";
    }
    std::string getInitInternalOptions() const {
        std::string internalOptions;
        initInternalOptions(internalOptions);
        return internalOptions;
    };
    void setConstantSurface(GraphicsAllocation *gfxAllocation) {
        if (gfxAllocation) {
            buildInfos[gfxAllocation->getRootDeviceIndex()].constantSurface = gfxAllocation;
        } else {
            for (auto &buildInfo : buildInfos) {
                buildInfo.constantSurface = nullptr;
            }
        }
    }
    void setGlobalSurface(GraphicsAllocation *gfxAllocation) {
        if (gfxAllocation) {
            buildInfos[gfxAllocation->getRootDeviceIndex()].globalSurface = gfxAllocation;
        } else {
            for (auto &buildInfo : buildInfos) {
                buildInfo.globalSurface = nullptr;
            }
        }
    }
    std::vector<KernelInfo *> &getKernelInfoArray(uint32_t rootDeviceIndex) {
        return buildInfos[rootDeviceIndex].kernelInfoArray;
    }
    void addKernelInfo(KernelInfo *inInfo, uint32_t rootDeviceIndex) {
        buildInfos[rootDeviceIndex].kernelInfoArray.push_back(inInfo);
    }
    std::vector<KernelInfo *> &getParentKernelInfoArray(uint32_t rootDeviceIndex) {
        return buildInfos[rootDeviceIndex].parentKernelInfoArray;
    }
    std::vector<KernelInfo *> &getSubgroupKernelInfoArray(uint32_t rootDeviceIndex) {
        return buildInfos[rootDeviceIndex].subgroupKernelInfoArray;
    }
    void setContext(Context *context) {
        this->context = context;
        contextSet = true;
    }
    void setSourceCode(const char *ptr) { sourceCode = ptr; }
    void clearOptions() { options = ""; }
    void setCreatedFromBinary(bool createdFromBin) { isCreatedFromBinary = createdFromBin; }
    void clearLog(uint32_t rootDeviceIndex) { buildInfos[rootDeviceIndex].buildLog.clear(); }

    void setIrBinary(char *ptr, bool isSpirv) {
        irBinary.reset(ptr);
        this->isSpirV = isSpirV;
    }
    void setIrBinarySize(size_t bsz, bool isSpirv) {
        irBinarySize = bsz;
        this->isSpirV = isSpirV;
    }

    std::string getCachedFileName() const;
    void setAllowNonUniform(bool allow) {
        allowNonUniform = allow;
    }

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
        setBuildStatus(CL_BUILD_NONE);
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtins;
        return this->build(getDevices(), this->options.c_str(), false, builtins);
    }

    void replaceDeviceBinary(std::unique_ptr<char[]> &&newBinary, size_t newBinarySize, uint32_t rootDeviceIndex) override {
        if (replaceDeviceBinaryCalledPerRootDevice.find(rootDeviceIndex) == replaceDeviceBinaryCalledPerRootDevice.end()) {
            replaceDeviceBinaryCalledPerRootDevice.insert({rootDeviceIndex, 1});
        } else {
            replaceDeviceBinaryCalledPerRootDevice[rootDeviceIndex]++;
        }
        Program::replaceDeviceBinary(std::move(newBinary), newBinarySize, rootDeviceIndex);
    }
    cl_int processGenBinary(const ClDevice &clDevice) override {
        auto rootDeviceIndex = clDevice.getRootDeviceIndex();
        if (processGenBinaryCalledPerRootDevice.find(rootDeviceIndex) == processGenBinaryCalledPerRootDevice.end()) {
            processGenBinaryCalledPerRootDevice.insert({rootDeviceIndex, 1});
        } else {
            processGenBinaryCalledPerRootDevice[rootDeviceIndex]++;
        }
        return Program::processGenBinary(clDevice);
    }

    void initInternalOptions(std::string &internalOptions) const override {
        initInternalOptionsCalled++;
        Program::initInternalOptions(internalOptions);
    };

    const KernelInfo &getKernelInfoForKernel(const char *kernelName) const {
        return *getKernelInfo(kernelName, getDevices()[0]->getRootDeviceIndex());
    }

    const KernelInfoContainer getKernelInfosForKernel(const char *kernelName) const {
        KernelInfoContainer kernelInfos;
        kernelInfos.resize(getMaxRootDeviceIndex() + 1);
        for (auto i = 0u; i < kernelInfos.size(); i++) {
            kernelInfos[i] = getKernelInfo(kernelName, i);
        }
        return kernelInfos;
    }

    std::map<uint32_t, int> processGenBinaryCalledPerRootDevice;
    std::map<uint32_t, int> replaceDeviceBinaryCalledPerRootDevice;
    static int initInternalOptionsCalled;
    bool contextSet = false;
    int isFlagOptionOverride = -1;
    int isOptionValueValidOverride = -1;
};

class GMockProgram : public Program {
  public:
    using Program::Program;
    MOCK_METHOD(bool, appendKernelDebugOptions, (ClDevice &, std::string &), (override));
};

} // namespace NEO

/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/program/program.h"

#include <string>

namespace NEO {

class GraphicsAllocation;
class BuiltinDispatchInfoBuilder;
class Context;

ClDeviceVector toClDeviceVector(ClDevice &clDevice);
////////////////////////////////////////////////////////////////////////////////
// Program - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockNeoProgram : public NEO::Program {
  public:
    using Base = NEO::Program;
    using Base::buildInfos;
    using Base::exportedFunctionsKernelId;

    MockNeoProgram(NEO::Context *context, bool isBuiltIn, const NEO::ClDeviceVector &devices)
        : NEO::Program(context, isBuiltIn, devices) {}

    void resizeAndPopulateKernelInfoArray(size_t size) {
        buildInfos[0].kernelInfoArray.resize(size);
        for (auto &entry : buildInfos[0].kernelInfoArray) {
            entry = new KernelInfo();
        }
    }
};

class MockProgram : public Program {
  public:
    using Program::allowNonUniform;
    using Program::areSpecializationConstantsInitialized;
    using Program::buildInfos;
    using Program::containsVmeUsage;
    using Program::context;
    using Program::createdFrom;
    using Program::createProgramFromBinary;
    using Program::debuggerInfos;
    using Program::deviceBuildInfos;
    using Program::disableZebinIfVmeEnabled;
    using Program::extractInternalOptions;
    using Program::getKernelInfo;
    using Program::getModuleAllocations;
    using Program::internalOptionsToExtract;
    using Program::irBinary;
    using Program::irBinarySize;
    using Program::isBuiltIn;
    using Program::isCreatedFromBinary;
    using Program::isSpirV;
    using Program::linkBinary;
    using Program::notifyModuleCreate;
    using Program::notifyModuleDestroy;
    using Program::options;
    using Program::packDeviceBinary;
    using Program::processGenBinaries;
    using Program::Program;
    using Program::requiresRebuild;
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
        this->requiresRebuild = true;
        setBuildStatus(CL_BUILD_NONE);
        std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> builtins;
        return this->build(getDevices(), this->options.c_str(), builtins);
    }

    cl_int recompile() {
        this->isCreatedFromBinary = false;
        this->requiresRebuild = true;
        setBuildStatus(CL_BUILD_NONE);
        return this->compile(getDevices(), this->options.c_str(), 0, nullptr, nullptr);
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

    std::string getInternalOptions() const override {
        getInternalOptionsCalled++;
        return Program::getInternalOptions();
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

    void processDebugData(uint32_t rootDeviceIndex) override {
        Program::processDebugData(rootDeviceIndex);
        wasProcessDebugDataCalled = true;
    }

    void createDebugZebin(uint32_t rootDeviceIndex) override {
        Program::createDebugZebin(rootDeviceIndex);
        wasCreateDebugZebinCalled = true;
    }

    void debugNotify(const ClDeviceVector &deviceVector, std::unordered_map<uint32_t, BuildPhase> &phasesReached) override {
        Program::debugNotify(deviceVector, phasesReached);
        wasDebuggerNotified = true;
    }

    void callPopulateZebinExtendedArgsMetadataOnce(uint32_t rootDeviceIndex) override {
        wasPopulateZebinExtendedArgsMetadataOnceCalled = true;

        if (callBasePopulateZebinExtendedArgsMetadataOnce) {
            return Program::callPopulateZebinExtendedArgsMetadataOnce(rootDeviceIndex);
        }
    }

    std::vector<NEO::ExternalFunctionInfo> externalFunctions;
    std::map<uint32_t, int> processGenBinaryCalledPerRootDevice;
    std::map<uint32_t, int> replaceDeviceBinaryCalledPerRootDevice;
    static int getInternalOptionsCalled;
    bool contextSet = false;
    int isFlagOptionOverride = -1;
    int isOptionValueValidOverride = -1;
    bool wasProcessDebugDataCalled = false;
    bool wasCreateDebugZebinCalled = false;
    bool wasDebuggerNotified = false;
    bool wasPopulateZebinExtendedArgsMetadataOnceCalled = false;
    bool callBasePopulateZebinExtendedArgsMetadataOnce = false;
};

} // namespace NEO

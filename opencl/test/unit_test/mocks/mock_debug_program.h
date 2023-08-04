/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/program.h"

#include <memory>

namespace NEO {
class ClDevice;
class ClDeviceVector;
class Device;
struct KernelInfo;
struct MockCompilerInterfaceCaptureBuildOptions;
} // namespace NEO

class MockDebugProgram : public NEO::Program {
  public:
    using Base = NEO::Program;
    using Base::Base;
    using Base::buildInfos;
    using Base::irBinary;
    using Base::irBinarySize;

    MockDebugProgram(const NEO::ClDeviceVector &deviceVector);

    void debugNotify(const NEO::ClDeviceVector &deviceVector, std::unordered_map<uint32_t, BuildPhase> &phasesReached) override;
    void createDebugZebin(uint32_t rootDeviceIndex) override;
    void processDebugData(uint32_t rootDeviceIndex) override;
    cl_int processGenBinary(const NEO::ClDevice &clDevice) override;

    void addKernelInfo(NEO::KernelInfo *inInfo, uint32_t rootDeviceIndex);

    NEO::KernelInfo *kernelInfo = nullptr;
    std::unique_ptr<char[]> kernelSsh;
    NEO::MockCompilerInterfaceCaptureBuildOptions *compilerInterface;
    bool wasDebuggerNotified = false;
    bool wasCreateDebugZebinCalled = false;
    bool wasProcessDebugDataCalled = false;

  protected:
    void prepareKernelInfo();
    void prepareSSHForDebugSurface();
    void prepareMockCompilerInterface(NEO::Device &device);
};

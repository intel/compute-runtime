/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/kernel/kernel.h"

namespace NEO {
template <>
struct OpenCLObjectMapper<_cl_kernel> {
    typedef class MultiDeviceKernel DerivedType;
};

using KernelVectorType = StackVec<Kernel *, 4>;

class MultiDeviceKernel : public BaseObject<_cl_kernel> {
  public:
    static const cl_ulong objectMagic = 0x3284ADC8EA0AFE25LL;

    ~MultiDeviceKernel() override;
    MultiDeviceKernel(KernelVectorType kernelVector);

    Kernel *getKernel(uint32_t rootDeviceIndex) const { return kernels[rootDeviceIndex]; }
    Kernel *getDefaultKernel() const { return defaultKernel; }

    template <typename kernel_t = Kernel, typename program_t = Program, typename multi_device_kernel_t = MultiDeviceKernel>
    static multi_device_kernel_t *create(program_t *program, const KernelInfoContainer &kernelInfos, cl_int *errcodeRet) {
        KernelVectorType kernels{};
        kernels.resize(program->getMaxRootDeviceIndex() + 1);

        for (auto &pDevice : program->getDevices()) {
            auto rootDeviceIndex = pDevice->getRootDeviceIndex();
            if (kernels[rootDeviceIndex]) {
                continue;
            }
            kernels[rootDeviceIndex] = Kernel::create<kernel_t, program_t>(program, kernelInfos, errcodeRet);
        }
        auto pMultiDeviceKernel = new multi_device_kernel_t(std::move(kernels));

        return pMultiDeviceKernel;
    }

    cl_int cloneKernel(Kernel *pSourceKernel) { return defaultKernel->cloneKernel(pSourceKernel); }
    const std::vector<Kernel::SimpleKernelArgInfo> &getKernelArguments() const { return defaultKernel->getKernelArguments(); }
    cl_int checkCorrectImageAccessQualifier(cl_uint argIndex, size_t argSize, const void *argValue) const { return defaultKernel->checkCorrectImageAccessQualifier(argIndex, argSize, argValue); }
    void unsetArg(uint32_t argIndex) { return defaultKernel->unsetArg(argIndex); }
    cl_int setArg(uint32_t argIndex, size_t argSize, const void *argVal) { return defaultKernel->setArg(argIndex, argSize, argVal); }
    cl_int getInfo(cl_kernel_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet); }
    cl_int getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet); }
    const ClDeviceVector &getDevices() const { return defaultKernel->getDevices(); }
    size_t getKernelArgsNumber() const { return defaultKernel->getKernelArgsNumber(); }
    Context &getContext() const { return defaultKernel->getContext(); }
    cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) { return defaultKernel->setArgSvmAlloc(argIndex, svmPtr, svmAlloc); }
    bool getHasIndirectAccess() const { return defaultKernel->getHasIndirectAccess(); }
    void setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) { return defaultKernel->setUnifiedMemoryProperty(infoType, infoValue); }
    void setSvmKernelExecInfo(GraphicsAllocation *argValue) { return defaultKernel->setSvmKernelExecInfo(argValue); }
    void clearSvmKernelExecInfo() { return defaultKernel->clearSvmKernelExecInfo(); }
    void setUnifiedMemoryExecInfo(GraphicsAllocation *argValue) { return defaultKernel->setUnifiedMemoryExecInfo(argValue); }
    void clearUnifiedMemoryExecInfo() { return defaultKernel->clearUnifiedMemoryExecInfo(); }
    int setKernelThreadArbitrationPolicy(uint32_t propertyValue) { return defaultKernel->setKernelThreadArbitrationPolicy(propertyValue); }
    cl_int setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) { return defaultKernel->setKernelExecutionType(executionType); }
    int32_t setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) { return defaultKernel->setAdditionalKernelExecInfoWithParam(paramName, paramValueSize, paramValue); }

  protected:
    KernelVectorType kernels;
    Kernel *defaultKernel = nullptr;
};

} // namespace NEO

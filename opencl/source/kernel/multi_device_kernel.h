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

class MultiDeviceKernel : public BaseObject<_cl_kernel> {
  public:
    static const cl_ulong objectMagic = 0x3284ADC8EA0AFE25LL;

    ~MultiDeviceKernel() override;
    MultiDeviceKernel(Kernel *pKernel);

    Kernel *getKernel(uint32_t rootDeviceIndex) const { return kernel; }
    Kernel *getDefaultKernel() const { return kernel; }

    template <typename kernel_t = Kernel, typename program_t = Program, typename multi_device_kernel_t = MultiDeviceKernel>
    static multi_device_kernel_t *create(program_t *program, const KernelInfoContainer &kernelInfos, cl_int *errcodeRet) {

        auto pKernel = Kernel::create<kernel_t, program_t>(program, kernelInfos, errcodeRet);
        auto pMultiDeviceKernel = new multi_device_kernel_t(pKernel);

        return pMultiDeviceKernel;
    }

    cl_int cloneKernel(Kernel *pSourceKernel) { return kernel->cloneKernel(pSourceKernel); }
    const std::vector<Kernel::SimpleKernelArgInfo> &getKernelArguments() const { return kernel->getKernelArguments(); }
    cl_int checkCorrectImageAccessQualifier(cl_uint argIndex, size_t argSize, const void *argValue) const { return kernel->checkCorrectImageAccessQualifier(argIndex, argSize, argValue); }
    void unsetArg(uint32_t argIndex) { return kernel->unsetArg(argIndex); }
    cl_int setArg(uint32_t argIndex, size_t argSize, const void *argVal) { return kernel->setArg(argIndex, argSize, argVal); }
    cl_int getInfo(cl_kernel_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return kernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet); }
    cl_int getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return kernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet); }
    const ClDeviceVector &getDevices() const { return kernel->getDevices(); }
    size_t getKernelArgsNumber() const { return kernel->getKernelArgsNumber(); }
    Context &getContext() const { return kernel->getContext(); }
    cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) { return kernel->setArgSvmAlloc(argIndex, svmPtr, svmAlloc); }
    bool getHasIndirectAccess() const { return kernel->getHasIndirectAccess(); }
    void setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) { return kernel->setUnifiedMemoryProperty(infoType, infoValue); }
    void setSvmKernelExecInfo(GraphicsAllocation *argValue) { return kernel->setSvmKernelExecInfo(argValue); }
    void clearSvmKernelExecInfo() { return kernel->clearSvmKernelExecInfo(); }
    void setUnifiedMemoryExecInfo(GraphicsAllocation *argValue) { return kernel->setUnifiedMemoryExecInfo(argValue); }
    void clearUnifiedMemoryExecInfo() { return kernel->clearUnifiedMemoryExecInfo(); }
    int setKernelThreadArbitrationPolicy(uint32_t propertyValue) { return kernel->setKernelThreadArbitrationPolicy(propertyValue); }
    cl_int setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) { return kernel->setKernelExecutionType(executionType); }
    int32_t setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) { return kernel->setAdditionalKernelExecInfoWithParam(paramName, paramValueSize, paramValue); }

  protected:
    Kernel *kernel = nullptr;
};

} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/multi_device_kernel.h"
namespace NEO {

MultiDeviceKernel::~MultiDeviceKernel() {
    for (auto &pKernel : kernels) {
        if (pKernel) {
            pKernel->decRefInternal();
        }
    }
}
MultiDeviceKernel::MultiDeviceKernel(KernelVectorType kernelVector) : kernels(std::move(kernelVector)) {
    for (auto &pKernel : kernels) {
        if (pKernel) {
            if (!defaultKernel) {
                defaultKernel = kernels[(*pKernel->getDevices().begin())->getRootDeviceIndex()];
            }
            pKernel->incRefInternal();
            pKernel->setMultiDeviceKernel(this);
        }
    }
};

const std::vector<Kernel::SimpleKernelArgInfo> &MultiDeviceKernel::getKernelArguments() const { return defaultKernel->getKernelArguments(); }
cl_int MultiDeviceKernel::getInfo(cl_kernel_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet); }
cl_int MultiDeviceKernel::getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet); }
const ClDeviceVector &MultiDeviceKernel::getDevices() const { return defaultKernel->getDevices(); }
size_t MultiDeviceKernel::getKernelArgsNumber() const { return defaultKernel->getKernelArgsNumber(); }
Context &MultiDeviceKernel::getContext() const { return defaultKernel->getContext(); }
bool MultiDeviceKernel::getHasIndirectAccess() const { return defaultKernel->getHasIndirectAccess(); }

cl_int MultiDeviceKernel::cloneKernel(Kernel *pSourceKernel) { return getResultFromEachKernel(&Kernel::cloneKernel, pSourceKernel); }
cl_int MultiDeviceKernel::checkCorrectImageAccessQualifier(cl_uint argIndex, size_t argSize, const void *argValue) const { return getResultFromEachKernel(&Kernel::checkCorrectImageAccessQualifier, argIndex, argSize, argValue); }
void MultiDeviceKernel::unsetArg(uint32_t argIndex) { callOnEachKernel(&Kernel::unsetArg, argIndex); }
cl_int MultiDeviceKernel::setArg(uint32_t argIndex, size_t argSize, const void *argVal) { return getResultFromEachKernel(&Kernel::setArgument, argIndex, argSize, argVal); }
cl_int MultiDeviceKernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc) { return getResultFromEachKernel(&Kernel::setArgSvmAlloc, argIndex, svmPtr, svmAlloc); }
void MultiDeviceKernel::setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) { callOnEachKernel(&Kernel::setUnifiedMemoryProperty, infoType, infoValue); }
void MultiDeviceKernel::setSvmKernelExecInfo(GraphicsAllocation *argValue) { callOnEachKernel(&Kernel::setSvmKernelExecInfo, argValue); }
void MultiDeviceKernel::clearSvmKernelExecInfo() { callOnEachKernel(&Kernel::clearSvmKernelExecInfo); }
void MultiDeviceKernel::setUnifiedMemoryExecInfo(GraphicsAllocation *argValue) { callOnEachKernel(&Kernel::setUnifiedMemoryExecInfo, argValue); }
void MultiDeviceKernel::clearUnifiedMemoryExecInfo() { callOnEachKernel(&Kernel::clearUnifiedMemoryExecInfo); }
int MultiDeviceKernel::setKernelThreadArbitrationPolicy(uint32_t propertyValue) { return getResultFromEachKernel(&Kernel::setKernelThreadArbitrationPolicy, propertyValue); }
cl_int MultiDeviceKernel::setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) { return getResultFromEachKernel(&Kernel::setKernelExecutionType, executionType); }
int32_t MultiDeviceKernel::setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) { return getResultFromEachKernel(&Kernel::setAdditionalKernelExecInfoWithParam, paramName, paramValueSize, paramValue); }

} // namespace NEO

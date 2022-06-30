/*
 * Copyright (C) 2021-2022 Intel Corporation
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

Kernel *MultiDeviceKernel::determineDefaultKernel(KernelVectorType &kernelVector) {
    for (auto &pKernel : kernelVector) {
        if (pKernel) {
            return pKernel;
        }
    }
    UNRECOVERABLE_IF(true);
    return nullptr;
}
MultiDeviceKernel::MultiDeviceKernel(KernelVectorType kernelVector, const KernelInfoContainer kernelInfosArg) : kernels(std::move(kernelVector)),
                                                                                                                defaultKernel(MultiDeviceKernel::determineDefaultKernel(kernels)),
                                                                                                                program(defaultKernel->getProgram()),
                                                                                                                kernelInfos(kernelInfosArg) {
    for (auto &pKernel : kernels) {
        if (pKernel) {
            pKernel->incRefInternal();
            pKernel->setMultiDeviceKernel(this);
        }
    }
};

const std::vector<Kernel::SimpleKernelArgInfo> &MultiDeviceKernel::getKernelArguments() const { return defaultKernel->getKernelArguments(); }
cl_int MultiDeviceKernel::getInfo(cl_kernel_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet); }
cl_int MultiDeviceKernel::getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const { return defaultKernel->getArgInfo(argIndx, paramName, paramValueSize, paramValue, paramValueSizeRet); }
const ClDeviceVector &MultiDeviceKernel::getDevices() const { return program->getDevicesInProgram(); }
size_t MultiDeviceKernel::getKernelArgsNumber() const { return defaultKernel->getKernelArgsNumber(); }
Context &MultiDeviceKernel::getContext() const { return defaultKernel->getContext(); }
bool MultiDeviceKernel::getHasIndirectAccess() const { return defaultKernel->getHasIndirectAccess(); }

cl_int MultiDeviceKernel::checkCorrectImageAccessQualifier(cl_uint argIndex, size_t argSize, const void *argValue) const { return getResultFromEachKernel(&Kernel::checkCorrectImageAccessQualifier, argIndex, argSize, argValue); }
void MultiDeviceKernel::unsetArg(uint32_t argIndex) { callOnEachKernel(&Kernel::unsetArg, argIndex); }
cl_int MultiDeviceKernel::setArg(uint32_t argIndex, size_t argSize, const void *argVal) { return getResultFromEachKernel(&Kernel::setArgument, argIndex, argSize, argVal); }
void MultiDeviceKernel::setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) { callOnEachKernel(&Kernel::setUnifiedMemoryProperty, infoType, infoValue); }
void MultiDeviceKernel::clearSvmKernelExecInfo() { callOnEachKernel(&Kernel::clearSvmKernelExecInfo); }
void MultiDeviceKernel::clearUnifiedMemoryExecInfo() { callOnEachKernel(&Kernel::clearUnifiedMemoryExecInfo); }
int MultiDeviceKernel::setKernelThreadArbitrationPolicy(uint32_t propertyValue) { return getResultFromEachKernel(&Kernel::setKernelThreadArbitrationPolicy, propertyValue); }
cl_int MultiDeviceKernel::setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) { return getResultFromEachKernel(&Kernel::setKernelExecutionType, executionType); }
int32_t MultiDeviceKernel::setAdditionalKernelExecInfoWithParam(uint32_t paramName, size_t paramValueSize, const void *paramValue) { return getResultFromEachKernel(&Kernel::setAdditionalKernelExecInfoWithParam, paramName, paramValueSize, paramValue); }

void MultiDeviceKernel::storeKernelArgAllocIdMemoryManagerCounter(uint32_t argIndex, uint32_t allocIdMemoryManagerCounter) {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pKernel = getKernel(rootDeviceIndex);
        if (pKernel) {
            pKernel->storeKernelArgAllocIdMemoryManagerCounter(argIndex, allocIdMemoryManagerCounter);
        }
    }
}

cl_int MultiDeviceKernel::cloneKernel(MultiDeviceKernel *pSourceMultiDeviceKernel) {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pSrcKernel = pSourceMultiDeviceKernel->getKernel(rootDeviceIndex);
        auto pDstKernel = getKernel(rootDeviceIndex);
        if (pSrcKernel) {
            pDstKernel->cloneKernel(pSrcKernel);
        }
    }
    return CL_SUCCESS;
}
cl_int MultiDeviceKernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, MultiGraphicsAllocation *svmAllocs, uint32_t allocId) {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pKernel = getKernel(rootDeviceIndex);
        if (pKernel) {
            if (svmAllocs && (svmAllocs->getGraphicsAllocations().size() <= rootDeviceIndex || !svmAllocs->getGraphicsAllocation(rootDeviceIndex))) {
                continue;
            }
            auto svmAlloc = svmAllocs ? svmAllocs->getGraphicsAllocation(rootDeviceIndex) : nullptr;
            pKernel->setArgSvmAlloc(argIndex, svmPtr, svmAlloc, allocId);
        }
    }
    return CL_SUCCESS;
}
void MultiDeviceKernel::setSvmKernelExecInfo(const MultiGraphicsAllocation &argValue) {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pKernel = getKernel(rootDeviceIndex);
        if (pKernel) {
            pKernel->setSvmKernelExecInfo(argValue.getGraphicsAllocation(rootDeviceIndex));
        }
    }
}
void MultiDeviceKernel::setUnifiedMemoryExecInfo(const MultiGraphicsAllocation &argValue) {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pKernel = getKernel(rootDeviceIndex);
        if (pKernel) {
            pKernel->setUnifiedMemoryExecInfo(argValue.getGraphicsAllocation(rootDeviceIndex));
        }
    }
}
} // namespace NEO

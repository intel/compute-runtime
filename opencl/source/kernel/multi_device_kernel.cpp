/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/multi_device_kernel.h"

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/program/program.h"

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
MemObj *MultiDeviceKernel::getMemObjArg(uint32_t argIndex, size_t argSize, const void *argVal) const {
    if (argVal == nullptr || argSize != sizeof(cl_mem)) {
        return nullptr;
    }
    const auto &kernelArguments = defaultKernel->getKernelArguments();
    if (argIndex >= kernelArguments.size()) {
        return nullptr;
    }
    const auto argType = kernelArguments[argIndex].type;
    if (argType != Kernel::BUFFER_OBJ && argType != Kernel::IMAGE_OBJ) {
        return nullptr;
    }
    return castToObject<MemObj>(*static_cast<const cl_mem *>(argVal));
}

cl_int MultiDeviceKernel::setArg(uint32_t argIndex, size_t argSize, const void *argVal) {
    auto pMemObj = getMemObjArg(argIndex, argSize, argVal);

    cl_int retVal = CL_INVALID_VALUE;
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < kernels.size(); rootDeviceIndex++) {
        auto pKernel = getKernel(rootDeviceIndex);
        if (pKernel) {
            if (pMemObj && !pMemObj->getMultiGraphicsAllocation().getGraphicsAllocation(rootDeviceIndex)) {
                pKernel->unsetArg(argIndex);
                continue;
            }
            retVal = pKernel->setArgument(argIndex, argSize, argVal);
            if (CL_SUCCESS != retVal) {
                break;
            }
        }
    }
    return retVal;
}
void MultiDeviceKernel::setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue) { callOnEachKernel(&Kernel::setUnifiedMemoryProperty, infoType, infoValue); }
void MultiDeviceKernel::clearSvmKernelExecInfo() { callOnEachKernel(&Kernel::clearSvmKernelExecInfo); }
void MultiDeviceKernel::clearUnifiedMemoryExecInfo() { callOnEachKernel(&Kernel::clearUnifiedMemoryExecInfo); }
int MultiDeviceKernel::setKernelThreadArbitrationPolicy(uint32_t propertyValue) { return getResultFromEachKernel(&Kernel::setKernelThreadArbitrationPolicy, propertyValue); }
cl_int MultiDeviceKernel::setKernelExecutionType(cl_execution_info_kernel_type_intel executionType) { return getResultFromEachKernel(&Kernel::setKernelExecutionType, executionType); }

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
            if (svmAllocs && !svmAllocs->getGraphicsAllocation(rootDeviceIndex)) {
                pKernel->unsetArg(argIndex);
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

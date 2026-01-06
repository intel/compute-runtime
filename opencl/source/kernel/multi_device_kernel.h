/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/external_functions.h"

#include "opencl/source/helpers/base_object.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {
class ClDeviceVector;
class MultiGraphicsAllocation;
class Context;
class MultiDeviceKernel;

template <>
struct OpenCLObjectMapper<_cl_kernel> {
    typedef class MultiDeviceKernel DerivedType;
};

using KernelVectorType = StackVec<Kernel *, 4>;
using KernelInfoContainer = StackVec<const KernelInfo *, 4>;

class MultiDeviceKernel : public BaseObject<_cl_kernel> {
  public:
    static const cl_ulong objectMagic = 0x3284ADC8EA0AFE25LL;

    ~MultiDeviceKernel() override;
    MultiDeviceKernel(KernelVectorType kernelVector, const KernelInfoContainer kernelInfosArg);

    Kernel *getKernel(uint32_t rootDeviceIndex) const { return kernels[rootDeviceIndex]; }
    Kernel *getDefaultKernel() const { return defaultKernel; }

    template <typename KernelType = Kernel, typename ProgramType = Program, typename MultiDeviceKernelType = MultiDeviceKernel>
    static MultiDeviceKernelType *create(ProgramType *program, const KernelInfoContainer &kernelInfos, cl_int &errcodeRet) {
        KernelVectorType kernels{};
        kernels.resize(program->getMaxRootDeviceIndex() + 1);

        for (auto &pDevice : program->getDevicesInProgram()) {
            auto rootDeviceIndex = pDevice->getRootDeviceIndex();
            if (kernels[rootDeviceIndex]) {
                continue;
            }
            kernels[rootDeviceIndex] = Kernel::create<KernelType, ProgramType>(program, *kernelInfos[rootDeviceIndex], *pDevice, errcodeRet);
            if (!kernels[rootDeviceIndex]) {
                return nullptr;
            }
        }
        auto pMultiDeviceKernel = new MultiDeviceKernelType(std::move(kernels), kernelInfos);

        return pMultiDeviceKernel;
    }

    cl_int cloneKernel(MultiDeviceKernel *pSourceMultiDeviceKernel);
    const std::vector<Kernel::SimpleKernelArgInfo> &getKernelArguments() const;
    cl_int checkCorrectImageAccessQualifier(cl_uint argIndex, size_t argSize, const void *argValue) const;
    void unsetArg(uint32_t argIndex);
    cl_int setArg(uint32_t argIndex, size_t argSize, const void *argVal);
    cl_int getInfo(cl_kernel_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;
    cl_int getArgInfo(cl_uint argIndx, cl_kernel_arg_info paramName, size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;
    const ClDeviceVector &getDevices() const;
    size_t getKernelArgsNumber() const;
    Context &getContext() const;
    cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, MultiGraphicsAllocation *svmAllocs, uint32_t allocId);
    bool getHasIndirectAccess() const;
    void setUnifiedMemoryProperty(cl_kernel_exec_info infoType, bool infoValue);
    void setSvmKernelExecInfo(const MultiGraphicsAllocation &argValue);
    void clearSvmKernelExecInfo();
    void setUnifiedMemoryExecInfo(const MultiGraphicsAllocation &argValue);
    void clearUnifiedMemoryExecInfo();
    int setKernelThreadArbitrationPolicy(uint32_t propertyValue);
    cl_int setKernelExecutionType(cl_execution_info_kernel_type_intel executionType);
    void storeKernelArgAllocIdMemoryManagerCounter(uint32_t argIndex, uint32_t allocIdMemoryManagerCounter);
    Program *getProgram() const { return program; }
    const KernelInfoContainer &getKernelInfos() const { return kernelInfos; }

  protected:
    template <typename FuncType, typename... Args>
    cl_int getResultFromEachKernel(FuncType function, Args &&...args) const {
        cl_int retVal = CL_INVALID_VALUE;

        for (auto &pKernel : kernels) {
            if (pKernel) {
                retVal = (pKernel->*function)(std::forward<Args>(args)...);
                if (CL_SUCCESS != retVal) {
                    break;
                }
            }
        }
        return retVal;
    }
    template <typename FuncType, typename... Args>
    void callOnEachKernel(FuncType function, Args &&...args) {
        for (auto &pKernel : kernels) {
            if (pKernel) {
                (pKernel->*function)(args...);
            }
        }
    }
    static Kernel *determineDefaultKernel(KernelVectorType &kernelVector);
    KernelVectorType kernels;
    Kernel *defaultKernel = nullptr;
    Program *program = nullptr;
    const KernelInfoContainer kernelInfos;
};

} // namespace NEO

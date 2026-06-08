/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/program/program.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

#include <map>

namespace NEO {
namespace LEO {

class Image;

template <>
struct OpenCLObjectMapper<_cl_kernel> {
    typedef class Kernel DerivedType;
};

class Kernel : public BaseObject<_cl_kernel> {
  public:
    static const cl_ulong objectMagic = 0x3284ADC8EA0AFE25LL;

    static ze_kernel_indirect_access_flags_t indirectAccessFlagToL0(cl_kernel_exec_info clFlag);
    static ze_scheduling_hint_exp_flags_t schedulingHintToL0(uint32_t arbitrationPolicy);

    Kernel(std::map<uint32_t, ze_kernel_handle_t> kernelHandles, Program *program);
    Kernel(Kernel *sourceKernel);
    Kernel() = delete;
    ~Kernel() override;

    cl_int getInfo(cl_kernel_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet) const;

    cl_int getWorkGroupInfo(cl_device_id device,
                            cl_kernel_work_group_info paramName,
                            size_t inputValueSize,
                            const void *inputValue,
                            size_t paramValueSize,
                            void *paramValue,
                            size_t *paramValueSizeRet) const;

    cl_int getArgInfo(cl_uint argIndex, cl_kernel_arg_info paramName, size_t paramValueSize,
                      void *paramValue, size_t *paramValueSizeRet) const;

    cl_int getSuggestedLocalWorkSize(cl_uint workDim, const size_t *globalWorkSize, size_t *suggestedLocalWorkSize);
    cl_int getMaxConcurrentWorkGroupCount(cl_uint workDim, const size_t *localWorkSize, size_t *suggestedWorkGroupCount);
    Context *getContext() const { return this->program->getContext(); }

    bool areAllArgsSet() const {
        return std::all_of(argsSet.begin(), argsSet.end(), [](bool val) { return val; });
    };
    void markArgAsSet(cl_uint argIndex) { this->argsSet[argIndex] = true; };
    size_t getNumArgs() const { return this->argsSet.size(); };

    cl_int setIndirectAccess(cl_kernel_exec_info flag, cl_bool val);
    cl_int setThreadArbitrationPolicy(uint32_t flag);

    cl_int setArgumentValue(uint32_t argIndex, size_t argSize, const void *argValue);

    void setImageArg(uint32_t argIndex, Image *image) { this->imageArgs[argIndex] = image; }
    void clearImageArg(uint32_t argIndex) { this->imageArgs.erase(argIndex); }
    const std::map<uint32_t, Image *> &getImageArgs() const { return this->imageArgs; }

    ze_kernel_handle_t getL0Handle() const { return this->kernelHandles.begin()->second; };
    ze_kernel_handle_t getL0Handle(uint32_t rootDeviceIndex) const {
        auto it = this->kernelHandles.find(rootDeviceIndex);
        return it == this->kernelHandles.end() ? this->kernelHandles.begin()->second : it->second;
    };
    L0::KernelImp *getL0Object() const { return static_cast<L0::KernelImp *>(L0::Kernel::fromHandle(this->getL0Handle())); };
    L0::KernelImp *getL0Object(uint32_t rootDeviceIndex) const { return static_cast<L0::KernelImp *>(L0::Kernel::fromHandle(this->getL0Handle(rootDeviceIndex))); };
    const std::map<uint32_t, ze_kernel_handle_t> &getKernelHandles() const { return this->kernelHandles; };

  protected:
    Kernel *clonedFromKernel = nullptr;

    std::vector<bool> argsSet{};
    std::map<uint32_t, ze_kernel_handle_t> kernelHandles{};
    std::map<uint32_t, Image *> imageArgs{};
    Program *program = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<Kernel>);

} // namespace LEO
} // namespace NEO

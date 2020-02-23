/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/vec.h"
#include "opencl/source/built_ins/built_ins.h"
#include "opencl/source/kernel/kernel.h"

#include "CL/cl.h"
#include "built_in_ops.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace NEO {
typedef std::vector<char> BuiltinResourceT;

class Context;
class Device;
class MemObj;
struct MultiDispatchInfo;
class Program;

struct BuiltinOpParams {
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    MemObj *srcMemObj = nullptr;
    MemObj *dstMemObj = nullptr;
    GraphicsAllocation *srcSvmAlloc = nullptr;
    GraphicsAllocation *dstSvmAlloc = nullptr;
    GraphicsAllocation *transferAllocation = nullptr; //mapAllocation or hostPtrAllocation
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::None;
    bool unifiedMemoryArgsRequireMemSync = true;
    Vec3<size_t> srcOffset = {0, 0, 0};
    Vec3<size_t> dstOffset = {0, 0, 0};
    Vec3<size_t> size = {0, 0, 0};
    size_t srcRowPitch = 0;
    size_t dstRowPitch = 0;
    size_t srcSlicePitch = 0;
    size_t dstSlicePitch = 0;
    uint32_t srcMipLevel = 0;
    uint32_t dstMipLevel = 0;
};

class BuiltinDispatchInfoBuilder {
  public:
    BuiltinDispatchInfoBuilder(BuiltIns &kernelLib) : kernelsLib(kernelLib) {}
    virtual ~BuiltinDispatchInfoBuilder() = default;

    template <typename... KernelsDescArgsT>
    void populate(Device &device, EBuiltInOps::Type operation, const char *options, KernelsDescArgsT &&... desc);

    virtual bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, const BuiltinOpParams &operationParams) const {
        return false;
    }

    virtual bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, Kernel *kernel,
                                    const uint32_t dim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const {
        return false;
    }

    virtual cl_int validateDispatch(Kernel *kernel, uint32_t inworkDim, const Vec3<size_t> &gws, const Vec3<size_t> &elws, const Vec3<size_t> &offset) const {
        return CL_SUCCESS;
    }

    // returns true if argument should be updated in kernel exposed to user code
    virtual bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const {
        err = 0;
        return true;
    }

    std::vector<std::unique_ptr<Kernel>> &peekUsedKernels() { return usedKernels; }

  protected:
    template <typename KernelNameT, typename... KernelsDescArgsT>
    void grabKernels(KernelNameT &&kernelName, Kernel *&kernelDst, KernelsDescArgsT &&... kernelsDesc) {
        const KernelInfo *kernelInfo = prog->getKernelInfo(kernelName);
        UNRECOVERABLE_IF(nullptr == kernelInfo);
        cl_int err = 0;
        kernelDst = Kernel::create(prog.get(), *kernelInfo, &err);
        kernelDst->isBuiltIn = true;
        usedKernels.push_back(std::unique_ptr<Kernel>(kernelDst));
        grabKernels(std::forward<KernelsDescArgsT>(kernelsDesc)...);
    }

    cl_int grabKernels() { return CL_SUCCESS; }

    std::unique_ptr<Program> prog;
    std::vector<std::unique_ptr<Kernel>> usedKernels;
    BuiltIns &kernelsLib;
};

class BuiltInDispatchBuilderOp {
  public:
    static BuiltinDispatchInfoBuilder &getBuiltinDispatchInfoBuilder(EBuiltInOps::Type op, Device &device);
    static BuiltinDispatchInfoBuilder &getUnknownDispatchInfoBuilder(EBuiltInOps::Type op, Device &device);
    std::unique_ptr<BuiltinDispatchInfoBuilder> setBuiltinDispatchInfoBuilder(EBuiltInOps::Type op, Device &device,
                                                                              std::unique_ptr<BuiltinDispatchInfoBuilder> newBuilder);
};

} // namespace NEO

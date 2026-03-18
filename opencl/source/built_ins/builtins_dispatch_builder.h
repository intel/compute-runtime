/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_in_ops_base.h"
#include "shared/source/command_stream/transfer_direction.h"
#include "shared/source/helpers/vec.h"

#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/program/program.h"

#include "CL/cl.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {
namespace BuiltIn {
struct Code;
using Resource = std::vector<char>;
} // namespace BuiltIn
class BuiltIns;
class GraphicsAllocation;
class Kernel;
struct KernelInfo;

class ClDeviceVector;
class Context;
class Device;
class MemObj;
struct MultiDispatchInfo;
class Program;

namespace BuiltIn {

struct OpParams {
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    MemObj *srcMemObj = nullptr;
    MemObj *dstMemObj = nullptr;
    GraphicsAllocation *srcSvmAlloc = nullptr;
    GraphicsAllocation *dstSvmAlloc = nullptr;
    GraphicsAllocation *transferAllocation = nullptr; // mapAllocation or hostPtrAllocation
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::none;
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
    void *userPtrForPostOperationCpuCopy = nullptr;
    bool bcsSplit = false;
    TransferDirection direction = TransferDirection::localToLocal;
};

class DispatchInfoBuilder {
  public:
    DispatchInfoBuilder(BuiltIns &kernelLib, ClDevice &device) : kernelsLib(kernelLib), clDevice(device) {}
    virtual ~DispatchInfoBuilder() = default;

    template <typename... KernelsDescArgsT>
    void populate(Group operation, ConstStringRef options, KernelsDescArgsT &&...desc);

    virtual bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo) const {
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

    std::vector<std::unique_ptr<MultiDeviceKernel>> &peekUsedKernels() { return usedKernels; }

    static std::unique_ptr<Program> createProgramFromCode(const Code &bc, const ClDeviceVector &device);

  protected:
    template <typename KernelNameT, typename... KernelsDescArgsT>
    void grabKernels(KernelNameT &&kernelName, MultiDeviceKernel *&kernelDst, KernelsDescArgsT &&...kernelsDesc) {
        auto rootDeviceIndex = clDevice.getRootDeviceIndex();
        const KernelInfo *kernelInfo = prog->getKernelInfo(kernelName, rootDeviceIndex);
        UNRECOVERABLE_IF(nullptr == kernelInfo);
        cl_int err = 0;
        KernelInfoContainer kernelInfos;
        kernelInfos.resize(rootDeviceIndex + 1);
        kernelInfos[rootDeviceIndex] = kernelInfo;
        kernelDst = MultiDeviceKernel::create(prog.get(), kernelInfos, err);
        usedKernels.push_back(std::unique_ptr<MultiDeviceKernel>(kernelDst));
        grabKernels(std::forward<KernelsDescArgsT>(kernelsDesc)...);
    }

    cl_int grabKernels() { return CL_SUCCESS; }

    std::unique_ptr<Program> prog;
    std::vector<std::unique_ptr<MultiDeviceKernel>> usedKernels;
    BuiltIns &kernelsLib;
    ClDevice &clDevice;
};

class DispatchBuilderOp {
  public:
    static DispatchInfoBuilder &getBuiltinDispatchInfoBuilder(Group builtInGroup, ClDevice &device);
};

class OwnershipWrapper : public NonCopyableAndNonMovableClass {
  public:
    OwnershipWrapper() = default;
    OwnershipWrapper(DispatchInfoBuilder &inputBuilder, Context *context);
    ~OwnershipWrapper();

    void takeOwnership(DispatchInfoBuilder &inputBuilder, Context *context);

  protected:
    DispatchInfoBuilder *builder = nullptr;
};

} // namespace BuiltIn

static_assert(NEO::NonCopyableAndNonMovable<BuiltIn::OwnershipWrapper>);

} // namespace NEO

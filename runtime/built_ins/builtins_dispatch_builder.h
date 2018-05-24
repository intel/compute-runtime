/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "CL/cl.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/kernel/kernel.h"
#include "runtime/utilities/vec.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace OCLRT {
typedef std::vector<char> BuiltinResourceT;

class Context;
class Device;
class MemObj;
struct MultiDispatchInfo;
class Program;

class BuiltinDispatchInfoBuilder {
  public:
    struct BuiltinOpParams {
        void *srcPtr = nullptr;
        void *dstPtr = nullptr;
        MemObj *srcMemObj = nullptr;
        MemObj *dstMemObj = nullptr;
        GraphicsAllocation *srcSvmAlloc = nullptr;
        GraphicsAllocation *dstSvmAlloc = nullptr;
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

    BuiltinDispatchInfoBuilder(BuiltIns &kernelLib) : kernelsLib(kernelLib) {}
    virtual ~BuiltinDispatchInfoBuilder() = default;

    template <typename... KernelsDescArgsT>
    void populate(Context &context, Device &device, EBuiltInOps operation, const char *options, KernelsDescArgsT &&... desc);

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
        return true;
    }

    void takeOwnership(Context *context);
    void releaseOwnership();

  protected:
    template <typename KernelNameT, typename... KernelsDescArgsT>
    void grabKernels(KernelNameT &&kernelName, Kernel *&kernelDst, KernelsDescArgsT &&... kernelsDesc) {
        const KernelInfo *ki = prog->getKernelInfo(kernelName);
        cl_int err = 0;
        kernelDst = Kernel::create(prog.get(), *ki, &err);
        kernelDst->isBuiltIn = true;
        usedKernels.push_back(std::unique_ptr<Kernel>(kernelDst));
        grabKernels(std::forward<KernelsDescArgsT>(kernelsDesc)...);
    }

    cl_int grabKernels() { return CL_SUCCESS; }

    std::unique_ptr<Program> prog;
    std::vector<std::unique_ptr<Kernel>> usedKernels;
    BuiltIns &kernelsLib;
};

} // namespace OCLRT

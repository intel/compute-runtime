/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_in_ops_base.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "implicit_args.h"

namespace L0 {
namespace ult {

struct MockBuiltInKernelLibImpl : BuiltInKernelLibImpl {

    using BuiltInKernelLibImpl::builtins;
    using BuiltInKernelLibImpl::getFunction;
    using BuiltInKernelLibImpl::imageBuiltins;
    using BuiltInKernelLibImpl::initAsyncComplete;

    MockBuiltInKernelLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltInKernelLibImpl(device, builtInsLib) {

        dummyKernel = std::unique_ptr<WhiteBox<::L0::KernelImp>>(new Mock<::L0::KernelImp>());
        dummyModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
        dummyKernel->module = dummyModule.get();
        mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    }
    void initBuiltinKernel(L0::BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        auto builtId = toBufferCacheIndex(func, mode);
        if (builtins[builtId].get() == nullptr) {
            builtins[builtId] = loadBuiltIn(NEO::BuiltIn::BaseKernel::copyBufferToBuffer, mode, "copyBufferToBufferBytesSingle");
        }
    }

    void initBuiltinImageKernel(L0::ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        auto builtId = toImageCacheIndex(func, mode);
        if (imageBuiltins[builtId].get() == nullptr) {
            imageBuiltins[builtId] = loadBuiltIn(NEO::BuiltIn::BaseKernel::copyImage3dToBuffer, mode, "CopyImage3dToBuffer16Bytes");
        }
    }

    std::unique_ptr<WhiteBox<::L0::KernelImp>> dummyKernel;
    std::unique_ptr<Module> dummyModule;
    std::unique_ptr<Module> mockModule;

    Kernel *getFunction(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        return dummyKernel.get();
    }

    Kernel *getImageFunction(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        return dummyKernel.get();
    }

    std::unique_ptr<BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::BaseKernel baseKernel, const NEO::BuiltIn::AddressingMode &mode, const char *kernelName) override {
        std::unique_ptr<Kernel> mockKernel(new Mock<::L0::KernelImp>());

        return std::unique_ptr<BuiltInKernelData>(new BuiltInKernelData{mockModule.get(), std::move(mockKernel)});
    }
};

struct MockCheckPassedArgumentsBuiltInKernelLibImpl : BuiltInKernelLibImpl {

    using BuiltInKernelLibImpl::builtins;

    MockCheckPassedArgumentsBuiltInKernelLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltInKernelLibImpl(device, builtInsLib) {
    }

    std::unique_ptr<BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::BaseKernel baseKernel, const NEO::BuiltIn::AddressingMode &mode, const char *kernelName) override {
        kernelNamePassed = kernelName;
        baseKernelPassed = baseKernel;

        return nullptr;
    }

    std::string kernelNamePassed;
    NEO::BuiltIn::BaseKernel baseKernelPassed;
};

} // namespace ult
} // namespace L0

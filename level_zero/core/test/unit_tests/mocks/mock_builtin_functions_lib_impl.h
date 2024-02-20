/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

    using BuiltinFunctionsLibImpl::builtins;
    using BuiltinFunctionsLibImpl::getFunction;
    using BuiltinFunctionsLibImpl::imageBuiltins;
    using BuiltinFunctionsLibImpl::initAsyncComplete;

    MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {

        dummyKernel = std::unique_ptr<WhiteBox<::L0::KernelImp>>(new Mock<::L0::KernelImp>());
        dummyModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
        dummyKernel->module = dummyModule.get();
        mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    }
    void initBuiltinKernel(L0::Builtin func) override {
        auto builtId = static_cast<uint32_t>(func);
        if (builtins[builtId].get() == nullptr) {
            builtins[builtId] = loadBuiltIn(NEO::EBuiltInOps::copyBufferToBuffer, "copyBufferToBufferBytesSingle");
        }
    }

    void initBuiltinImageKernel(L0::ImageBuiltin func) override {
        auto builtId = static_cast<uint32_t>(func);
        if (imageBuiltins[builtId].get() == nullptr) {
            imageBuiltins[builtId] = loadBuiltIn(NEO::EBuiltInOps::copyImage3dToBuffer, "CopyImage3dToBuffer16Bytes");
        }
    }

    std::unique_ptr<WhiteBox<::L0::KernelImp>> dummyKernel;
    std::unique_ptr<Module> dummyModule;
    std::unique_ptr<Module> mockModule;

    Kernel *getFunction(Builtin func) override {
        return dummyKernel.get();
    }

    Kernel *getImageFunction(ImageBuiltin func) override {
        return dummyKernel.get();
    }

    std::unique_ptr<BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
        std::unique_ptr<Kernel> mockKernel(new Mock<::L0::KernelImp>());

        return std::unique_ptr<BuiltinData>(new BuiltinData{mockModule.get(), std::move(mockKernel)});
    }
};

struct MockCheckPassedArgumentsBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

    using BuiltinFunctionsLibImpl::builtins;

    MockCheckPassedArgumentsBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {
    }

    std::unique_ptr<BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
        kernelNamePassed = builtInName;
        builtinPassed = builtin;

        return nullptr;
    }

    std::string kernelNamePassed;
    NEO::EBuiltInOps::Type builtinPassed;
};

} // namespace ult
} // namespace L0

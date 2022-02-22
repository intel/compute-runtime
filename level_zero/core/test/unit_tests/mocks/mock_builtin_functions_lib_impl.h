/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct MockBuiltinFunctionsLibImpl : BuiltinFunctionsLibImpl {

    using BuiltinFunctionsLibImpl::builtins;
    using BuiltinFunctionsLibImpl::getFunction;
    using BuiltinFunctionsLibImpl::imageBuiltins;
    MockBuiltinFunctionsLibImpl(L0::Device *device, NEO::BuiltIns *builtInsLib) : BuiltinFunctionsLibImpl(device, builtInsLib) {

        dummyKernel = std::unique_ptr<WhiteBox<::L0::Kernel>>(new Mock<::L0::Kernel>());
        dummyModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
        dummyKernel->module = dummyModule.get();
    }
    void initBuiltinKernel(L0::Builtin func) override {
        auto builtId = static_cast<uint32_t>(func);
        if (builtins[builtId].get() == nullptr) {
            builtins[builtId] = loadBuiltIn(NEO::EBuiltInOps::CopyBufferToBuffer, "copyBufferToBufferBytesSingle");
        }
    }

    void initBuiltinImageKernel(L0::ImageBuiltin func) override {
        auto builtId = static_cast<uint32_t>(func);
        if (imageBuiltins[builtId].get() == nullptr) {
            imageBuiltins[builtId] = loadBuiltIn(NEO::EBuiltInOps::CopyImage3dToBuffer, "CopyImage3dToBuffer16Bytes");
        }
    }

    std::unique_ptr<WhiteBox<::L0::Kernel>> dummyKernel;
    std::unique_ptr<Module> dummyModule;

    Kernel *getFunction(Builtin func) override {
        return dummyKernel.get();
    }

    Kernel *getImageFunction(ImageBuiltin func) override {
        return dummyKernel.get();
    }

    std::unique_ptr<BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
        std::unique_ptr<Kernel> mockKernel(new Mock<::L0::Kernel>());
        std::unique_ptr<Module> mockModule(new Mock<Module>(device, nullptr));

        return std::unique_ptr<BuiltinData>(new BuiltinData{std::move(mockModule), std::move(mockKernel)});
    }
};
} // namespace ult
} // namespace L0
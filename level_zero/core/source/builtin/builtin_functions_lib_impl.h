/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/module/module.h"

namespace NEO {
namespace EBuiltInOps {
using Type = uint32_t;
}
class BuiltIns;
} // namespace NEO

namespace L0 {
struct BuiltinFunctionsLibImpl : BuiltinFunctionsLib {
    struct BuiltinData;
    BuiltinFunctionsLibImpl(Device *device, NEO::BuiltIns *builtInsLib)
        : device(device), builtInsLib(builtInsLib) {
    }
    ~BuiltinFunctionsLibImpl() override {
        builtins->reset();
        imageBuiltins->reset();
    }

    Kernel *getFunction(Builtin func) override;
    Kernel *getImageFunction(ImageBuiltin func) override;
    void initBuiltinKernel(Builtin builtId) override;
    void initBuiltinImageKernel(ImageBuiltin func) override;
    MOCKABLE_VIRTUAL std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName);

  protected:
    std::unique_ptr<BuiltinData> builtins[static_cast<uint32_t>(Builtin::COUNT)];
    std::unique_ptr<BuiltinData> imageBuiltins[static_cast<uint32_t>(ImageBuiltin::COUNT)];
    Device *device;
    NEO::BuiltIns *builtInsLib;
};
struct BuiltinFunctionsLibImpl::BuiltinData {
    MOCKABLE_VIRTUAL ~BuiltinData() {
        func.reset();
        module.reset();
    }
    BuiltinData() = default;
    BuiltinData(std::unique_ptr<L0::Module> &&mod, std::unique_ptr<L0::Kernel> &&ker) {
        module = std::move(mod);
        func = std::move(ker);
    }

    std::unique_ptr<Module> module;
    std::unique_ptr<Kernel> func;
};
} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
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
        pageFaultBuiltin.reset();
        imageBuiltins->reset();
    }

    Kernel *getFunction(Builtin func) override;
    Kernel *getImageFunction(ImageBuiltin func) override;
    Kernel *getPageFaultFunction() override;
    void initFunctions() override;
    void initImageFunctions() override;
    void initPageFaultFunction() override;
    std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName);

  protected:
    std::unique_ptr<BuiltinData> builtins[static_cast<uint32_t>(Builtin::COUNT)];
    std::unique_ptr<BuiltinData> imageBuiltins[static_cast<uint32_t>(ImageBuiltin::COUNT)];
    std::unique_ptr<BuiltinData> pageFaultBuiltin;

    Device *device;
    NEO::BuiltIns *builtInsLib;
};
struct BuiltinFunctionsLibImpl::BuiltinData {
    ~BuiltinData() {
        func.reset();
        module.reset();
    }

    std::unique_ptr<Module> module;
    std::unique_ptr<Kernel> func;
};
} // namespace L0

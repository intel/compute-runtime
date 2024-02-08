/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/module/module.h"

#include <future>
#include <vector>

namespace NEO {
namespace EBuiltInOps {
using Type = uint32_t;
}
class BuiltIns;
} // namespace NEO

namespace L0 {
struct Kernel;
struct Device;

struct BuiltinFunctionsLibImpl : BuiltinFunctionsLib {
    struct BuiltinData;
    BuiltinFunctionsLibImpl(Device *device, NEO::BuiltIns *builtInsLib);
    ~BuiltinFunctionsLibImpl() override {
        this->ensureInitCompletionImpl();
        builtins->reset();
        imageBuiltins->reset();
    }

    Kernel *getFunction(Builtin func) override;
    Kernel *getImageFunction(ImageBuiltin func) override;
    void initBuiltinKernel(Builtin builtId) override;
    void initBuiltinImageKernel(ImageBuiltin func) override;
    void ensureInitCompletion() override;
    void ensureInitCompletionImpl();
    MOCKABLE_VIRTUAL std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName);

    static bool initBuiltinsAsyncEnabled(Device *device);

  protected:
    std::vector<std::unique_ptr<Module>> modules = {};
    std::unique_ptr<BuiltinData> builtins[static_cast<uint32_t>(Builtin::count)];
    std::unique_ptr<BuiltinData> imageBuiltins[static_cast<uint32_t>(ImageBuiltin::count)];
    Device *device;
    NEO::BuiltIns *builtInsLib;

    bool initAsyncComplete = true;
    std::atomic_bool initAsync = false;
};
struct BuiltinFunctionsLibImpl::BuiltinData {
    MOCKABLE_VIRTUAL ~BuiltinData();
    BuiltinData();
    BuiltinData(Module *module, std::unique_ptr<L0::Kernel> &&ker);

    Module *module = nullptr;
    std::unique_ptr<Kernel> func;
};
} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/builtin/builtin_functions_lib.h"

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
        pageFaultBuiltin.release();
    }

    Kernel *getFunction(Builtin func) override;
    Kernel *getPageFaultFunction() override;
    void initFunctions() override;
    void initPageFaultFunction() override;
    std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName);

  protected:
    std::unique_ptr<BuiltinData> builtins[static_cast<uint32_t>(Builtin::COUNT)];
    std::unique_ptr<BuiltinData> pageFaultBuiltin;

    Device *device;
    NEO::BuiltIns *builtInsLib;
};

} // namespace L0

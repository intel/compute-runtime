/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"

namespace L0 {
namespace ult {

struct MockBuiltinDataTimestamp : BuiltinFunctionsLibImpl::BuiltinData {
    using BuiltinFunctionsLibImpl::BuiltinData::BuiltinData;
};
struct MockBuiltinFunctionsLibImplTimestamps : BuiltinFunctionsLibImpl {

    using BuiltinFunctionsLibImpl::BuiltinFunctionsLibImpl;

    void initBuiltinKernel(Builtin func) override {
        auto builtId = static_cast<uint32_t>(func);
        switch (static_cast<Builtin>(func)) {
        case Builtin::queryKernelTimestamps:
        case Builtin::queryKernelTimestampsStateless:
        case Builtin::queryKernelTimestampsStatelessHeapless:
            if (builtins[builtId].get() == nullptr) {
                builtins[builtId] = loadBuiltIn(NEO::EBuiltInOps::queryKernelTimestamps, "QueryKernelTimestamps");
            }
            break;
        case Builtin::queryKernelTimestampsWithOffsets:
        case Builtin::queryKernelTimestampsWithOffsetsStateless:
        case Builtin::queryKernelTimestampsWithOffsetsStatelessHeapless:
            if (builtins[builtId].get() == nullptr) {
                builtins[builtId] = loadBuiltIn(NEO::EBuiltInOps::queryKernelTimestamps, "QueryKernelTimestampsWithOffsets");
            }
            break;
        default:
            break;
        };
    }

    void initBuiltinImageKernel(ImageBuiltin func) override {
    }

    Kernel *getFunction(Builtin func) override {
        auto builtId = static_cast<uint32_t>(func);
        return builtins[builtId]->func.get();
    }

    std::unique_ptr<BuiltinFunctionsLibImpl::BuiltinData> loadBuiltIn(NEO::EBuiltInOps::Type builtin, const char *builtInName) override {
        using BuiltInCodeType = NEO::BuiltinCode::ECodeType;

        auto builtInCodeType = NEO::debugManager.flags.RebuildPrecompiledKernels.get() ? BuiltInCodeType::intermediate : BuiltInCodeType::binary;
        auto builtInCode = builtInsLib->getBuiltinsLib().getBuiltinCode(builtin, builtInCodeType, *device->getNEODevice());

        [[maybe_unused]] ze_result_t res;

        L0::Module *module;
        ze_module_handle_t moduleHandle;
        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = builtInCode.type == BuiltInCodeType::binary ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<uint8_t *>(&builtInCode.resource[0]);
        moduleDesc.inputSize = builtInCode.resource.size();
        res = device->createModule(&moduleDesc, &moduleHandle, nullptr, ModuleType::builtin);
        UNRECOVERABLE_IF(res != ZE_RESULT_SUCCESS);
        module = Module::fromHandle(moduleHandle);

        std::unique_ptr<Kernel> kernel;
        ze_kernel_handle_t kernelHandle;
        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = builtInName;
        res = module->createKernel(&kernelDesc, &kernelHandle);
        DEBUG_BREAK_IF(res != ZE_RESULT_SUCCESS);

        kernel.reset(Kernel::fromHandle(kernelHandle));
        return std::unique_ptr<BuiltinData>(new MockBuiltinDataTimestamp{module, std::move(kernel)});
    }
};

} // namespace ult
} // namespace L0